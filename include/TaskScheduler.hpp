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


    //在timermap中添加函数
    int SetTimerFunc(UINT64 num,std::string_view name,std::function<void()>&& func)
    {
        if (m_timerFunctionMapPtr->find(num)!=m_timerFunctionMapPtr->end())
        {
            (*m_timerFunctionMapPtr)[num][std::string(name)]=std::make_shared<std::function<void()>>(func);
        }
        else
        {
            m_timerFunctionMapPtr->insert(std::make_pair(num, std::map<std::string, std::shared_ptr<std::function<void()>>>()));
            (*m_timerFunctionMapPtr)[num][std::string(name)] = std::make_shared<std::function<void()>>(func);
        }
        return 0;
    }
    int GetTimerFunc(std::shared_ptr<ThreadSafeMap<UINT64, std::map<std::string, std::shared_ptr<std::function<void()>>>>>& FuncMapPtr)
    {
        FuncMapPtr = m_timerFunctionMapPtr;

        return 0;
    }
    //在rpcmap中添加函数
    int SetRPCFunc(std::string_view funcName, std::function<int(const std::string&, std::string&)>&& func)
    {
        if (m_rpcFunctionMapPtr->find(std::string(funcName)) != m_rpcFunctionMapPtr->end())
        {
            (*m_rpcFunctionMapPtr)[std::string(funcName)] = std::make_shared<std::function<int(const std::string&, std::string&)>>(func);

        }
        else
        {
            m_rpcFunctionMapPtr->insert(std::make_pair(std::string(funcName), std::make_shared<std::function<int(const std::string&,std::string&)>>(func)));
        }
        return 0;
    }

    //从rpcmap中获得函数
    int GetRPCFunc(std::string_view funcName,std::function<int(const std::string&, std::string&)>& funcPtr)
    {
        if (m_rpcFunctionMapPtr->find(std::string(funcName)) != m_rpcFunctionMapPtr->end())
        {
            funcPtr = *(*m_rpcFunctionMapPtr)[std::string(funcName)];
            return 0;
        }
        else
        {
            return 1;
        }
    }

    //在anymap中添加函数,any 为std::Function
    int SetDllFunc(std::string_view funcName,std::any& func)
    {
        if (m_dllFunctionMapPtr->find(std::string(funcName)) != m_dllFunctionMapPtr->end())
        {
             (*m_dllFunctionMapPtr)[std::string(funcName)]=std::make_shared<std::any>(func) ;
            
        }
        else
        {
            m_dllFunctionMapPtr->insert(std::make_pair(std::string(funcName), std::make_shared<std::any>(func)));
        }
    }

    //从anymap中获得函数
    int GetDllFunc(std::string_view funcName, std::any& func)
    {
        if (m_dllFunctionMapPtr->find(std::string(funcName))!=m_dllFunctionMapPtr->end())
        {
            func = (*(*m_dllFunctionMapPtr)[std::string(funcName)]);
            return 0;
        }
        else
        {
            return 1;
        }
    }
    //添加消息
    int AddMessage(std::string_view queueName,std::shared_ptr<MessageBase>&& messageBasePtr)
    {
        auto it =m_messageQueueMapPtr->find(std::string(queueName));
        if (it != m_messageQueueMapPtr->end())
        {             
            (*m_messageQueueMapPtr)[std::string(queueName)].push(messageBasePtr);
        }
        else
        {           
            ThreadSafePriorityQueue<std::shared_ptr<MessageBase>> threadSafeQueue;
            m_messageQueueMapPtr->insert(std::make_pair(queueName, threadSafeQueue));
        }
        return 0;
    }
    //设置处理函数
    int SetAsyncFunc(std::string_view MessageType, int Number, std::string_view Name, std::function<void(std::shared_ptr<MessageBase>)>&& Func)
    {
        MessageTask new1(std::string(Name), Func);

        if (m_messageDealMapPtr->find(std::string(MessageType)) != m_messageDealMapPtr->end())
        {
            (*m_messageDealMapPtr)[std::string(MessageType)][Number] = new1;
        }
        else
        {
            std::map<int, MessageTask> newMap = std::map<int, MessageTask>();
            newMap.insert(std::make_pair(Number, new1));
            m_messageDealMapPtr->insert(std::make_pair(std::string(MessageType), newMap));
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
    //运行任务
    void runTask()
    {
        //开启死循环处理任务
        runTask_();
    }
    //定时方法
    std::shared_ptr<ThreadSafeMap<UINT64, std::map<std::string,std::shared_ptr<std::function<void()>>>>> m_timerFunctionMapPtr;
    //函数map,用于RPC
    std::shared_ptr<ThreadSafeMap<std::string, std::shared_ptr<std::function<int(const std::string&, std::string&)>>>> m_rpcFunctionMapPtr;
    //函数map,用于DLL,any为std::shared_ptr<Func>
    std::shared_ptr<ThreadSafeMap<std::string, std::shared_ptr<std::any>>> m_dllFunctionMapPtr;
    //将消息放入不同类型的消息队列
    std::shared_ptr<std::unordered_map<std::string, ThreadSafePriorityQueue<std::shared_ptr<MessageBase>> >> m_messageQueueMapPtr;
    //不同类型消息的处理函数所有函数都把信息提前bind
    //线程安全
    std::shared_ptr<ThreadSafeMap<std::string, std::map<int,MessageTask>>> m_messageDealMapPtr;
    std::shared_ptr<spdlog::logger> logger;
    
private:

    void runTask_()
    {

        while (1)
        {
                        //非永久任务执行
            for (std::unordered_map<std::string, ThreadSafePriorityQueue<std::shared_ptr<MessageBase>>>::iterator it = m_messageQueueMapPtr->begin(); it != m_messageQueueMapPtr->end(); ++it)
            {
                if (it->second.empty())
                {
                    continue;
                }
                else
                {   //调用前把函数取出来避免出错
                    std::shared_ptr<std::map<int, MessageTask>> TaskMapPtr = std::make_shared<std::map<int, MessageTask>>((*m_messageDealMapPtr)[it->second.top()->type]);
                    std::function<void()> taskRun = [&it, this, TaskMapPtr]()
                        {
                            for (std::map<int, MessageTask>::iterator taskIt = TaskMapPtr->begin(); taskIt != TaskMapPtr->end(); taskIt++)
                            {                            
                                //判断在执行前是否超时
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
                                //判断执行后是否超时                         
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
                    //返回结果
                    it->second.pop();
                }
            }



        }
    }
};
#endif
