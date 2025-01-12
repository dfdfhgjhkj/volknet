#ifndef AGENTBASE_HPP
#define AGENTBASE_HPP
#include <iostream>
#include <map>
#include <any>
#include <functional>
#include <string>
#include "MessageBase.hpp"
#include "rapidxml/rapidxml.hpp"
#include "spdlog/spdlog.h"
#include "spdlog/sinks/basic_file_sink.h"
#include <ThreadSafeMap.hpp>
#include <nlohmann/json.hpp>
#ifdef _MSC_VER
#define DLLEXPORT __declspec(dllexport)
#endif
#ifndef _MSC_VER
#define DLLEXPORT 
#endif
class AgentBase
{    

public:    
        //设置定时函数，第一个值为毫秒数
        std::function<int(UINT64,std::string_view name,  std::function<void()>&&)> m_setTimerFuncPtr;
        //查看修改定时函数
        std::function<int(std::shared_ptr<ThreadSafeMap<UINT64, std::map<std::string, std::shared_ptr<std::function<void()>>>>>&)> m_getTimerFuncPtr;
        //添加rpc函数,any为std::shared_ptr<std::Function>
        std::function<int(std::string_view funcName, std::function<int(const std::string&, std::string&)>&&)>  m_setRPCFuncPtr;
        //获得rpc函数
        std::function<int(std::string_view funcName, std::function<int(const std::string&, std::string&)>&)>  m_getRPCFuncPtr;
        //添加dll函数,any为std::shared_ptr<std::Function>
        std::function<int(std::string_view funcName, std::any&)>  m_setDllFuncPtr;
        //获得dll函数
        std::function<int(std::string_view funcName, std::any&)>  m_getDllFuncPtr;
        //添加消息
        std::function<int(std::string_view queueName, std::shared_ptr<MessageBase> )> m_pushMessagePtr;
        //添加消息处理函数
        std::function<int(std::string_view, int, std::string_view, std::function<void(std::shared_ptr<MessageBase>)>&&)>  m_pushCallbackFuncPtr;
        //logger
        std::shared_ptr<spdlog::logger> m_loggerPtr;
        //name
        std::string m_agentName;
        AgentBase();
        virtual ~AgentBase();
        //初始化
        virtual void initialize();
        //运行
        virtual void run();
     private:
};

AgentBase::AgentBase()
{

}

AgentBase::~AgentBase()
{

}

void AgentBase::initialize()
{
}
void AgentBase::run()
{
}
#define GetAgent(type) extern "C" {DLLEXPORT AgentBase* CreateAgent(){return (AgentBase*)(new type());}; DLLEXPORT void DeleteAgent(AgentBase* oldAgent){if(oldAgent){delete oldAgent;}return;};}

#define RegisterType(type) \
typeid(type).name(),\
std::make_shared<std::function<void(std::any&, std::string&)>>(\
    [&](std::any& anyData, std::string& str)\
    {\
        try\
        {\
            type typeData= std::any_cast<type>(anyData);\
            nlohmann::json j_vec(typeData);\
            str = j_vec.dump();\
            return;\
        }\
        catch (const std::exception& e)\
        {\
            spdlog::error("any cast fail{}", e.what());\
            return;\
        }\
    }\
),\
std::make_shared<std::function<void(std::any&, std::string&)>>(\
    [&](std::any& anyData, std::string& str)\
    {\
        try\
        {\
             anyData = std::make_any<type>(nlohmann::json::parse(str).get<type>());\
        }\
        catch (const std::exception& e)\
        {\
            spdlog::error("any cast fail{}", e.what()); \
            return; \
        }\
    }\
)\

#endif
