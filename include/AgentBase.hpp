#ifndef AGENTBASE_HPP
#define AGENTBASE_HPP
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

#include <boost/interprocess/managed_shared_memory.hpp>
#include <boost/interprocess/containers/vector.hpp>
#include <boost/interprocess/containers/string.hpp>
#include <boost/interprocess/containers/map.hpp>
#include <boost/interprocess/allocators/allocator.hpp>
#include <boost/interprocess/sync/named_recursive_mutex.hpp>
#ifdef _MSC_VER
#define DLLEXPORT __declspec(dllexport)
#endif
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
        std::function<int(UINT64,std::string_view name,  std::function<void()>)> m_setTimerFunc;
        //查看修改定时函数
        std::function<int(std::shared_ptr<ThreadSafeMap<UINT64, std::map<std::string, std::shared_ptr<std::function<void()>>>>>&)> m_getTimerFunc;
        //添加rpc函数
        std::function<int(std::string_view funcName, std::function<int(const std::string&, std::string&)>)>  m_setRPCFunc;
        //获得rpc函数
        std::function<int(std::string_view funcName, std::function<int(const std::string&, std::string&)>&)>  m_getRPCFunc;
        //添加dll函数,any为std::shared_ptr<std::Function>
        std::function<int(std::string_view funcName, std::any)>  m_setDllFunc;
        //获得dll函数
        std::function<int(std::string_view funcName, std::any&)>  m_getDllFunc;
        //添加消息
        std::function<int(std::string_view queueName, std::shared_ptr<MessageBase> )> m_pushMessageFunc;
        //添加消息处理函数
        std::function<int(std::string_view, int, std::string_view, std::function<void(std::shared_ptr<MessageBase>)>)>  m_pushCallbackFunc;
        //logger
        std::shared_ptr<spdlog::logger> m_loggerPtr;
        //name
        std::string m_agentName;
        std::map<std::string, std::shared_ptr<ButtonRPC>> m_RPCServerMap;
        std::map<std::string, std::shared_ptr<ButtonRPC>> m_RPCClientMap;

        std::shared_ptr <boost::interprocess::managed_shared_memory> m_segmentPtr;
        std::shared_ptr <CharAllocator> m_charallocPtr;
        std::shared_ptr <StringAllocator> m_strallocPtr;
        std::shared_ptr <MapAllocator> m_mapallocPtr;
        ShmemMap* m_mapPtr;
        std::shared_ptr<boost::interprocess::named_recursive_mutex > m_namedMtx;

        AgentBase();
        virtual ~AgentBase();
        //初始化
        virtual void initialize();
        //运行
        virtual void run();

        template <typename T>
        int subscribe(std::string_view name, T& data);

        template <typename T>
        int publish(std::string_view name, const T& data);
        void RegisterType_(const char* a, std::shared_ptr<std::function<std::string(Serializer)>> toStrFuncPtr, std::shared_ptr<std::function<Serializer(std::string)>> strToFuncPtr)
        {
            m_RPCServerMap["net"]->regist(std::string(a) + "ToStr", *toStrFuncPtr);
            m_RPCServerMap["shm"]->regist(std::string(a) + "ToStr", *toStrFuncPtr);
            m_RPCServerMap["net"]->regist(std::string(a) + "ToSe", *strToFuncPtr);
            m_RPCServerMap["shm"]->regist(std::string(a) + "ToSe", *strToFuncPtr);
        }

     private:
};

AgentBase::AgentBase()
{

}

AgentBase::~AgentBase()
{

}
template <typename T>
int AgentBase::subscribe(std::string_view name, T& data)
{
    Serializer dataSe;
    ShmemString nameSS(name, *m_charallocPtr);
    if (m_mapPtr->find(nameSS)==m_mapPtr->end())
    {         

        for (std::map<std::string, std::shared_ptr<ButtonRPC>>::iterator it = m_RPCClientMap.begin(); it != m_RPCClientMap.end(); it++)
        {
            std::string ip = it->first.substr(0, it->first.find(':'));
            if (ip != "shm")
            {        
                Serializer res;
                res=it->second->call("subscribe_net", dataSe);
                if (res.size()!=0)
                {
                    res >> data;
                    return 0;
                }
            }

        }
        return 1;
    }
    else

    {    
        ShmemString dataSS(*m_charallocPtr);
        m_namedMtx->lock();
        dataSS = m_mapPtr->at(nameSS);
        m_namedMtx->unlock();
        std::string typename_;
        dataSe.input(dataSS.data(), dataSS.size());
        dataSe >> typename_;
        if (typename_ != typeid(T).name())
        {
            return 1;
        }
        dataSe >> data;
        return 0;
    }

}




template <typename T>
int AgentBase::publish(std::string_view name, const T& data)
{

    Serializer dataSe;
    dataSe << typeid(T).name();
    dataSe << data;
    ShmemString nameSS(name, *m_charallocPtr);
    ShmemString dataSS(dataSe.data(), dataSe.size(), *m_charallocPtr);
    MapKVType pair_(nameSS, dataSS);

    m_namedMtx->lock();
    m_mapPtr->insert_or_assign(nameSS, dataSS);
    m_namedMtx->unlock();

    for (std::map<std::string, std::shared_ptr<ButtonRPC>>::iterator it = m_RPCClientMap.begin(); it != m_RPCClientMap.end(); it++)
    {
        std::string ip = it->first.substr(0, it->first.find(':'));
        if (ip!="shm")
        {
            it->second->call("publish_net",dataSe);
        }

    }
    return 0;
}

void AgentBase::initialize()
{
}
void AgentBase::run()
{
}
#define GetAgent(type) extern "C" {DLLEXPORT AgentBase* CreateAgent(){return (AgentBase*)(new type());}; DLLEXPORT void DeleteAgent(AgentBase* oldAgent){if(oldAgent){delete oldAgent;}return;};}

#define RegisterType(type) \
RegisterType_(\
typeid(type).name(),\
std::make_shared<std::function<std::string(Serializer)>>(\
    [&](Serializer anyData)\
    {\
        try\
        {\
            type typeData;\
            std::string typename_;\
            anyData>>typename_;\
            anyData>>typeData;\
            return nlohmann::json(typeData).dump();\
        }\
        catch (const std::exception& e)\
        {\
            spdlog::error("cast fail{}", e.what());\
            return std::string("cast fail");\
        }\
    }\
),\
std::make_shared<std::function<Serializer(std::string)>>(\
    [&](std::string str)\
    {\
        try\
        {\
            Serializer se; \
            se << (nlohmann::json::parse(str).get<type>()); \
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
