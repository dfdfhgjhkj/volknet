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
        //���ö�ʱ��������һ��ֵΪ������
        std::shared_ptr<std::function<int(const UINT64&& num,const std::string&& name,  std::function<void()>&)>> m_setTimerFuncPtr;
        //�鿴�޸Ķ�ʱ����
        std::shared_ptr<std::function<int(std::shared_ptr<ThreadSafeMap<UINT64, std::map<std::string, std::shared_ptr<std::function<void()>>>>>&)>> m_getTimerFuncPtr;
        //���rpc����,anyΪstd::shared_ptr<std::Function>
        std::shared_ptr<std::function<int(const std::string&& funcName, std::function<int(const std::string&, std::string&)>&)>>  m_setRPCFuncPtr;
        //���rpc����
        std::shared_ptr<std::function<int(const std::string&& funcName, std::function<int(const std::string&, std::string&)>&)>>  m_getRPCFuncPtr;
        //���dll����,anyΪstd::shared_ptr<std::Function>
        std::shared_ptr<std::function<int(const std::string&& funcName, std::any&)>>  m_setDllFuncPtr;
        //���dll����
        std::shared_ptr<std::function<int(const std::string&& funcName, std::any&)>>  m_getDllFuncPtr;
        //�����Ϣ
        std::shared_ptr<std::function<int(const std::string&& queueName, std::shared_ptr<MessageBase>& )>> m_pushMessagePtr;
        //�����Ϣ������
        std::shared_ptr<std::function<int(const std::string&&, const int&&, const std::string&&, std::function<void(std::shared_ptr<MessageBase>)>&)>>  m_pushCallbackFuncPtr;
        //logger
        std::shared_ptr<spdlog::logger> m_loggerPtr;
        //name
        std::string m_agentName;
        AgentBase();
        virtual ~AgentBase();
        //��ʼ��
        virtual void initialize();
        //����
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