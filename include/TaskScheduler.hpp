#ifndef TASKSCHEDULER_HPP
#define TASKSCHEDULER_HPP
#include <future>
#include <iostream>
#include <any>
#include <queue>
#include <mutex>
#include <map>
#include <unordered_map>
#include <condition_variable>
#include <thread>
#include "ThreadSafeMap.hpp"
#include "Task.hpp"
#include "ThreadSafePriorityQueue.hpp"
#include <spdlog/spdlog.h>
#include <spdlog/sinks/basic_file_sink.h>

class TaskScheduler
{

public:


    //��timermap�����Ӻ���
    int SetTimerFunc(const UINT64&& num,const std::string&& name,std::function<void()>& func)
    {
        if (m_timerFunctionMapPtr->find(num)!=m_timerFunctionMapPtr->end())
        {
            (*m_timerFunctionMapPtr)[num][name]=std::make_shared<std::function<void()>>(func);
        }
        else
        {
            m_timerFunctionMapPtr->insert(std::make_pair(num, std::map<std::string, std::shared_ptr<std::function<void()>>>()));
            (*m_timerFunctionMapPtr)[num][name] = std::make_shared<std::function<void()>>(func);
        }
        return 0;
    }
    int GetTimerFunc(std::shared_ptr<ThreadSafeMap<UINT64, std::map<std::string, std::shared_ptr<std::function<void()>>>>>& FuncMapPtr)
    {
        FuncMapPtr = m_timerFunctionMapPtr;

        return 0;
    }
    //��rpcmap�����Ӻ���
    int SetRPCFunc(const std::string&& funcName, std::function<int(const std::string&, std::string&)>& func)
    {
        if (m_rpcFunctionMapPtr->find(funcName) != m_rpcFunctionMapPtr->end())
        {
            (*m_rpcFunctionMapPtr)[funcName] = std::make_shared<std::function<int(const std::string&, std::string&)>>(func);

        }
        else
        {
            m_rpcFunctionMapPtr->insert(std::make_pair(funcName, std::make_shared<std::function<int(const std::string&,std::string&)>>(func)));
        }
        return 0;
    }

    //��rpcmap�л�ú���
    int GetRPCFunc(const std::string&& funcName,std::function<int(const std::string&, std::string&)>& funcPtr)
    {
        if (m_rpcFunctionMapPtr->find(funcName) != m_rpcFunctionMapPtr->end())
        {
            funcPtr = *(*m_rpcFunctionMapPtr)[funcName];
            return 0;
        }
        else
        {
            return 1;
        }
    }

    //��anymap�����Ӻ���,any Ϊstd::Function
    int SetDllFunc(const std::string&& funcName,std::any& func)
    {
        if (m_dllFunctionMapPtr->find(funcName) != m_dllFunctionMapPtr->end())
        {
             (*m_dllFunctionMapPtr)[funcName]=std::make_shared<std::any>(func) ;
            
        }
        else
        {
            m_dllFunctionMapPtr->insert(std::make_pair(funcName, std::make_shared<std::any>(func)));
        }
    }

    //��anymap�л�ú���
    int GetDllFunc(const std::string&& funcName, std::any& func)
    {
        if (m_dllFunctionMapPtr->find(funcName)!=m_dllFunctionMapPtr->end())
        {
            func = (*(*m_dllFunctionMapPtr)[funcName]);
            return 0;
        }
        else
        {
            return 1;
        }
    }
    //������Ϣ
    int AddMessage(const std::string&& queueName,std::shared_ptr<MessageBase>& messageBasePtr)
    {
        auto it =m_messageQueueMapPtr->find(queueName); 
        if (it != m_messageQueueMapPtr->end())
        {             
            (*m_messageQueueMapPtr)[queueName].push(messageBasePtr);
        }
        else
        {           
            ThreadSafePriorityQueue<std::shared_ptr<MessageBase>> threadSafeQueue;
            m_messageQueueMapPtr->insert(std::make_pair(queueName, threadSafeQueue));
        }
        return 0;
    }
    //���ô�������
    int SetAsyncFunc(const std::string&& MessageType, const int&& Number, const std::string&& Name, std::function<void(std::shared_ptr<MessageBase>)>& Func)
    {
        MessageTask new1(Name, Func);

        if (m_messageDealMapPtr->find(MessageType) != m_messageDealMapPtr->end())
        {
            (*m_messageDealMapPtr)[MessageType][Number] = new1;
        }
        else
        {
            std::map<int, MessageTask> newMap = std::map<int, MessageTask>();
            newMap.insert(std::make_pair(Number, new1));
            m_messageDealMapPtr->insert(std::make_pair(MessageType, newMap));
        }
        return 0;
    }
    TaskScheduler(std::shared_ptr<spdlog::logger> logger)
    {
        m_timerFunctionMapPtr = std::make_shared<ThreadSafeMap<UINT64, std::map<std::string, std::shared_ptr<std::function<void()>>>>>();
        m_dllFunctionMapPtr = std::make_shared<ThreadSafeMap<std::string, std::shared_ptr<std::any>>>();
        m_rpcFunctionMapPtr = std::make_shared<ThreadSafeMap<std::string, std::shared_ptr<std::function<int(const std::string&, std::string&)>>>>();
        m_messageQueueMapPtr = std::make_shared<std::unordered_map<std::string, ThreadSafePriorityQueue<std::shared_ptr<MessageBase>>>>();
        m_messageDealMapPtr = std::make_shared<ThreadSafeMap<std::string, std::map<int, MessageTask>>>();
        this->logger = logger;
    }
    ~TaskScheduler()
    {

    }
    //��������
    void runTask()
    {
        //������ѭ����������
        runTask_();
    }
    //��ʱ����
    std::shared_ptr<ThreadSafeMap<UINT64, std::map<std::string,std::shared_ptr<std::function<void()>>>>> m_timerFunctionMapPtr;
    //����map,����RPC
    std::shared_ptr<ThreadSafeMap<std::string, std::shared_ptr<std::function<int(const std::string&, std::string&)>>>> m_rpcFunctionMapPtr;
    //����map,����DLL,anyΪstd::shared_ptr<Func>
    std::shared_ptr<ThreadSafeMap<std::string, std::shared_ptr<std::any>>> m_dllFunctionMapPtr;
    //����Ϣ���벻ͬ���͵���Ϣ����
    std::shared_ptr<std::unordered_map<std::string, ThreadSafePriorityQueue<std::shared_ptr<MessageBase>> >> m_messageQueueMapPtr;
    //��ͬ������Ϣ�Ĵ����������к���������Ϣ��ǰbind
    //�̰߳�ȫ
    std::shared_ptr<ThreadSafeMap<std::string, std::map<int,MessageTask>>> m_messageDealMapPtr;
    std::shared_ptr<spdlog::logger> logger;
    
private:

    void runTask_()
    {

        while (1)
        {
                        //����������ִ��
            for (std::unordered_map<std::string, ThreadSafePriorityQueue<std::shared_ptr<MessageBase>>>::iterator it = m_messageQueueMapPtr->begin(); it != m_messageQueueMapPtr->end(); ++it)
            {
                if (it->second.empty())
                {
                    continue;
                }
                else
                {   //����ǰ�Ѻ���ȡ�����������
                    std::shared_ptr<std::map<int, MessageTask>> TaskMapPtr = std::make_shared<std::map<int, MessageTask>>((*m_messageDealMapPtr)[it->second.top()->type]);
                    std::function<void()> taskRun = [&it, this, TaskMapPtr]()
                        {
                            for (std::map<int, MessageTask>::iterator taskIt = TaskMapPtr->begin(); taskIt != TaskMapPtr->end(); taskIt++)
                            {                            
                                //�ж���ִ��ǰ�Ƿ�ʱ
                                std::chrono::microseconds duration_time_before = std::chrono::duration_cast<std::chrono::microseconds>(it->second.top()->m_finish - std::chrono::high_resolution_clock::now());
                                if (duration_time_before.count() < 0)
                                {
                                    spdlog::info("task serial number:{} task name:{} after task run time:{} timeout", std::to_string(taskIt->first), taskIt->second.MessageTaskName, std::to_string(duration_time_before.count()));
                                }
                                else
                                {
                                    spdlog::info("task serial number:{} task name:{} after task run time:{} no timeout", std::to_string(taskIt->first), taskIt->second.MessageTaskName, std::to_string(duration_time_before.count()));
                                }
                                (*(taskIt->second.Function))(it->second.top());                              
                                //�ж�ִ�к��Ƿ�ʱ                         
                                std::chrono::microseconds duration_time_after = std::chrono::duration_cast<std::chrono::microseconds>(it->second.top()->m_finish - std::chrono::high_resolution_clock::now());
                                if (duration_time_after.count() < 0)
                                {
                                    spdlog::info("task serial number:{} task name:{} after task run time:{} timeout", std::to_string(taskIt->first), taskIt->second.MessageTaskName, std::to_string(duration_time_after.count()));
                                }                                
                                else
                                {
                                    spdlog::info("task serial number:{} task name:{} after task run time:{} no timeout", std::to_string(taskIt->first), taskIt->second.MessageTaskName, std::to_string(duration_time_after.count()));
                                }
                             
                            }                            
                        };

                    std::future<void> futureResult = std::async(std::launch::async, taskRun);
                    futureResult.get();
                    //���ؽ��
                    it->second.pop();
                }
            }



        }
    }
};
#endif