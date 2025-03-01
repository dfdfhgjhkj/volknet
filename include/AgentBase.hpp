#ifndef AGENTBASE_HPP
#define AGENTBASE_HPP

#include <mutex>
#include <iostream>
#include <map>
#include <any>
#include <functional>
#include <string>
#include <MessageBase.hpp>
#include <rapidxml/rapidxml.hpp>
#include <spdlog/spdlog.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <ThreadSafeMap.hpp>
#include <nlohmann/json.hpp>
#include <ButtonRPC.hpp>
#include <chrono>
#include <boost/interprocess/managed_shared_memory.hpp>
#include <boost/interprocess/containers/vector.hpp>
#include <boost/interprocess/containers/string.hpp>
#include <boost/interprocess/containers/map.hpp>
#include <boost/interprocess/allocators/allocator.hpp>
#include <boost/interprocess/sync/named_recursive_mutex.hpp>
#ifdef _MSC_VER
#define DLLEXPORT __declspec(dllexport)
#endif
#include <shared_mutex>
#ifndef _MSC_VER
#define DLLEXPORT 
#endif





//using namespace boost::interprocess;
typedef boost::interprocess::allocator<char, boost::interprocess::managed_shared_memory::segment_manager> CharAllocator;
typedef boost::interprocess::basic_string<char, std::char_traits<char>, CharAllocator> ShmemString;
typedef boost::interprocess::allocator<ShmemString, boost::interprocess::managed_shared_memory::segment_manager> StringAllocator;
typedef boost::interprocess::vector<ShmemString, StringAllocator> ShmemVector;
typedef std::pair<const ShmemString, ShmemString> MapKVType;
typedef boost::interprocess::allocator<MapKVType, boost::interprocess::managed_shared_memory::segment_manager> MapAllocator;
typedef boost::interprocess::map< ShmemString, ShmemString, std::less<ShmemString>, MapAllocator>  ShmemMap;
typedef boost::interprocess::map< ShmemString, ShmemString, std::less<ShmemString>, MapAllocator>::iterator MapIterator;



class AgentBase
{    

public:    
        //设置定时函数，第一个值为毫秒数
        std::function<int(UINT64,std::string_view name,  std::function<void()>)> m_addTimerFunc;
        //查看修改定时函数时间间隔
        std::function<int(UINT64 num, std::string_view name)> m_alterTimer;
        //添加rpc函数
        std::function<int(std::string_view funcName, std::function<int(const std::string&, std::string&)>)>  m_setRPCFunc;
        //获得rpc函数
        std::function<int(std::string_view funcName, std::function<int(const std::string&, std::string&)>&)>  m_getRPCFunc;
        //添加dll函数
        std::function<int(std::string_view funcName, std::any)>  m_setDllFunc;
        //获得dll函数
        std::function<int(std::string_view funcName, std::any&)>  m_getDllFunc;
        //添加消息
        std::function<int(std::string_view queueName, std::shared_ptr<MessageBase> )> m_pushMessage;
        //添加消息处理函数
        std::function<int(std::string_view, int, std::string_view, std::function<void(std::shared_ptr<MessageBase>)>)>  m_pushCallBackFunc;
        //注册类型
        std::function<void(size_t a, const char* b, std::function<std::string(Serializer)>&& toStrFunc, std::function<Serializer(std::string)>&& strToFunc)> m_registeType;
        //subscribe
        std::function<void(std::string_view name, Serializer& dataSe)> m_subscribe;
        //publish
        std::function<void(std::string_view name, Serializer& dataSe)> m_publish;
        //logger
        std::shared_ptr<spdlog::logger> m_loggerPtr;
        //name
        std::string m_agentName;
        std::map<std::string, std::shared_ptr<ButtonRPC>> m_RPCServerMap;
        std::map<std::string, std::shared_ptr<ButtonRPC>> m_RPCClientMap;
        ShmemMap* m_mapPtr;
        std::shared_ptr<ThreadSafeMap<size_t, std::string>> m_typenameMapPtr;

        AgentBase();
        virtual ~AgentBase();
        //初始化
        virtual void initialize();
        //运行
        virtual void run();

        template <typename T>
        std::time_t subscribe(std::string_view name, T& data);

        template <typename T>
        std::time_t publish(std::string_view name, const T& data);

     private:
         std::shared_mutex m_mutex;
};

AgentBase::AgentBase()
{

}

AgentBase::~AgentBase()
{

}
template <typename T>
std::time_t AgentBase::subscribe(std::string_view name, T& data)
{
    std::time_t pubTime;
    size_t typename_;
    if (m_typenameMapPtr->find(typeid(T).hash_code()) == m_typenameMapPtr->end())
    {
        spdlog::error("typename {} have not been Registed.", typeid(T).name());
        return 0;
    }
    Serializer dataSe;
    m_subscribe(name, dataSe);

    if (dataSe.size()==0)
    {
        return 0;
    }
    else
    {
        dataSe >> typename_;
        if (typename_ != typeid(T).hash_code())
        {
            return 0;
        }
        dataSe >> data;
        dataSe >> pubTime;
        return pubTime;
    }
}




template <typename T>
std::time_t AgentBase::publish(std::string_view name, const T& data)
{


    if (m_typenameMapPtr->find(typeid(T).hash_code()) == m_typenameMapPtr->end())
    {
        spdlog::error("typename {} have not been Registed.", typeid(T).name()); 
        return 0;
    }
    std::time_t pubTime= std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
    Serializer dataSe;
    dataSe << typeid(T).hash_code();
    dataSe << data;

    dataSe << pubTime;
    m_publish(name,dataSe);


    return pubTime;
}

void AgentBase::initialize()
{
}
void AgentBase::run()
{
}
#define GetAgent(type) extern "C" {DLLEXPORT AgentBase* CreateAgent(){return (AgentBase*)(new type());}; DLLEXPORT void DeleteAgent(AgentBase* oldAgent){if(oldAgent){delete oldAgent;}return;};}

#define RegisteType(type) \
m_registeType(\
typeid(type).hash_code(),\
typeid(type).name(),\
std::function<std::string(Serializer)>(\
    [&](Serializer anyData)\
    {\
        try\
        {\
            type typeData;\
            size_t typename_;\
            anyData>>typename_;\
            nlohmann::json json_(typeData);\
            anyData>>typeData;\
            std::time_t pusTime;\
            anyData>>pusTime;\
            json_["pubTime"]=(long long)pusTime;\
            return json_.dump();\
        }\
        catch (const std::exception& e)\
        {\
            spdlog::error("cast fail{}", e.what());\
            return std::string("cast fail");\
        }\
    }\
),\
std::function<Serializer(std::string)>(\
    [&](std::string str)\
    {\
        try\
        {\
            Serializer se; \
            se<<typeid(type).hash_code();\
            nlohmann::json json_= nlohmann::json::parse(str);\
            std::time_t pubTime=json_["pubTime"].get<std::time_t>();\
            json_.erase("pubTime");\
            se << (nlohmann::json::parse(str).get<type>()); \
            se<<pubTime;\
            return se; \
        }\
        catch (const std::exception& e)\
        {\
            Serializer se;\
            spdlog::error("cast fail{}", e.what()); \
            return se; \
        }\
    }\
))\

#endif
