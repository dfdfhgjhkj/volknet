#include <future>
#include <iostream>
#include <boost/asio.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/asio/io_service.hpp>
#include <boost/bind/bind.hpp>
#include <fstream>

#include <queue>
#include <mutex>
#include <map>
#include <unordered_map>
#include <condition_variable>
#include <thread>
#include <DllLoad.hpp>
#include <TaskScheduler.hpp>
#include <AgentBase.hpp>
#include <rapidxml/rapidxml.hpp>
#include <spdlog/spdlog.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/async.h>
#include <spdlog/sinks/rotating_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>

//timer count
uint64_t count;
boost::asio::io_context timer_ioc;
//timer
boost::asio::deadline_timer timer(boost::asio::deadline_timer(timer_ioc, boost::posix_time::microsec(1000)));

std::shared_ptr<ThreadSafeMap<UINT64, std::map<std::string, std::shared_ptr<std::function<void()>>>>> timerFuncMapPtr;

void timeout()
{
    timer.expires_at(timer.expires_at() + boost::posix_time::microsec(1000));
    for (ThreadSafeMap<UINT64, std::map<std::string, std::shared_ptr<std::function<void()>>>>::iterator it = timerFuncMapPtr->begin(); it != timerFuncMapPtr->end(); it++)
    {
        if (count % it->first == 0)
        {
            for (std::map<std::string, std::shared_ptr<std::function<void()>>>::iterator it1 = it->second.begin(); it1 != it->second.end(); it1++)
            {
                std::future<void> futureResult = std::async(std::launch::async, *it1->second);
                std::future_status status = futureResult.wait_for(std::chrono::nanoseconds(it->first * 1000));
                switch (status)
                {
                case std::future_status::deferred:
                    break;
                case std::future_status::timeout:
                    break;
                case std::future_status::ready:
                    break;
                }
            }

        }

    }
    count++;
    timer.async_wait(boost::bind(&timeout));
}

void timer_run()
{
    try
    {
        timer.async_wait(boost::bind(&timeout));
        timer_ioc.run();

    }
    catch (std::exception& e)
    {
    }
}

int main()
{
    std::shared_ptr<spdlog::details::thread_pool> tp = std::make_shared<spdlog::details::thread_pool>(1,1);
    //文件接收机
    std::shared_ptr < spdlog::sinks::rotating_file_sink<std::mutex>> file_sink = std::make_shared<spdlog::sinks::rotating_file_sink_mt>("logs/logfile.log", 500 * 1024 * 1024, 1000);
    // 创建控制台（终端）接收器
    std::shared_ptr<spdlog::sinks::stdout_color_sink_mt> console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
    // 创建一个多重接收器的 logger
    std::vector<spdlog::sink_ptr> sinks{ console_sink, file_sink };
    std::shared_ptr<spdlog::async_logger> loggerPtr = std::make_shared<spdlog::async_logger>("multi_sink", sinks.begin(), sinks.end(),tp);

    // 设置全局 logger
    spdlog::set_default_logger(loggerPtr);
    // 设置日志消息格式
    spdlog::set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%^%l%$] [thread %t] %v");

    // 设置全局日志等级为 debug，所有日志都会输出
    spdlog::set_level(spdlog::level::debug);

    // 开启日志刷新
    spdlog::flush_on(spdlog::level::info);

    spdlog::info("VOLKNET online.");
    spdlog::info("Establishing battlefield control, standby.");
    spdlog::info("battlefield control online.");    
    std::shared_ptr<TaskScheduler> taskScheduler = std::make_shared<TaskScheduler>(loggerPtr);

    //设置定时函数，第一个值为毫秒数
    std::shared_ptr<std::function<int(UINT64 num,std::string_view name,  std::function<void()>)>> 
        setTimerFuncPtr=
        std::make_shared<std::function<int(UINT64 num,std::string_view name,  std::function<void()>)>>
        (std::bind(&TaskScheduler::SetTimerFunc,taskScheduler,std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
    //获得定时函数map
        //设置定时函数，第一个值为毫秒数
    std::shared_ptr<std::function<int(std::shared_ptr<ThreadSafeMap<UINT64, std::map<std::string, std::shared_ptr<std::function<void()>>>>>&)>>
        getTimerFuncPtr =
        std::make_shared<std::function<int(std::shared_ptr<ThreadSafeMap<UINT64, std::map<std::string, std::shared_ptr<std::function<void()>>>>>&)>>
        (std::bind(&TaskScheduler::GetTimerFunc, taskScheduler, std::placeholders::_1));

    //添加rpc函数
    std::shared_ptr<std::function<int(std::string_view funcName, std::function<int(const std::string&, std::string&)>)>>
        setRPCFuncPtr=
        std::make_shared<std::function<int(std::string_view funcName, std::function<int(const std::string&, std::string&)>)>>
        (std::bind(&TaskScheduler::SetRPCFunc, taskScheduler, std::placeholders::_1, std::placeholders::_2));
    //获得rpc函数
    std::shared_ptr<std::function<int(std::string_view funcName, std::function<int(const std::string&, std::string&)>&)>>
        getRPCFuncPtr=
        std::make_shared<std::function<int(std::string_view funcName, std::function<int(const std::string&, std::string&)>&)>>
        (std::bind(&TaskScheduler::GetRPCFunc, taskScheduler, std::placeholders::_1, std::placeholders::_2));
    //添加dll函数,any为std::shared_ptr<std::Function>
    std::shared_ptr<std::function<int(std::string_view funcName, std::any)>>  
        setDllFuncPtr=
        std::make_shared<std::function<int(std::string_view funcName, std::any)>>
        (std::bind(&TaskScheduler::SetDllFunc, taskScheduler, std::placeholders::_1, std::placeholders::_2));
    //获得dll函数
    std::shared_ptr<std::function<int(std::string_view funcName, std::any&)>>  
        getDllFuncPtr=
        std::make_shared<std::function<int(std::string_view funcName, std::any&)>>
        (std::bind(&TaskScheduler::GetDllFunc, taskScheduler, std::placeholders::_1, std::placeholders::_2));
    //添加消息函数
    std::shared_ptr<std::function<int(std::string_view queueName, std::shared_ptr<MessageBase>)>>
        pushMessagePtr =
        std::make_shared<std::function<int(std::string_view queueName, std::shared_ptr<MessageBase>)>>
        (std::bind(&TaskScheduler::AddMessage, taskScheduler, std::placeholders::_1, std::placeholders::_2));
    //添加消息处理函数
    std::shared_ptr<std::function<int(std::string_view, int, std::string_view, std::function<void(std::shared_ptr<MessageBase>)>)>>  
        pushCallbackFuncPtr=
        std::make_shared<std::function<int(std::string_view,int, std::string_view, std::function<void(std::shared_ptr<MessageBase>)>)>>
        (std::bind(&TaskScheduler::SetAsyncFunc, taskScheduler, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3, std::placeholders::_4));

    spdlog::flush_every(std::chrono::seconds(5)); // 定期刷新日志缓冲区

    rapidxml::xml_document<> doc;
    std::ifstream file("Agents/Agents.xml");
    if (!file) 
    {
        spdlog::error("Failed to open the Agents XML file.");
        return 1;
    }
    // 读取XML文件内容到内存
    std::shared_ptr<std::string> xml_contents=std::make_shared<std::string>((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    file.close();
    // 解析XML文档
    doc.parse<rapidxml::parse_default>(&(*xml_contents)[0]);
    // 获取根元素
    rapidxml::xml_node<>* root = doc.first_node("Agents");

    std::vector<std::shared_ptr<AgentBase>> agentPtrVector;

    std::shared_ptr<DllFuncLoader> dllFuncLoaderPtr= std::make_shared<DllFuncLoader>();

    std::map<std::string, std::shared_ptr<ButtonRPC>> RPCServerMap;
    std::map<std::string, std::shared_ptr<ButtonRPC>> RPCClientMap;

    for (rapidxml::xml_node<>* book = root->first_node("RPCServer"); book; book = book->next_sibling("RPCServer"))
    {
        for (rapidxml::xml_node<>* addr_node = book->first_node("addr"); addr_node; addr_node = addr_node->next_sibling("addr"))
        {
            if (addr_node)
            {
                std::string addr_ = addr_node->value();
                std::string ip = addr_.substr(0, addr_.find(':'));
                int port = std::atoi(addr_.substr(addr_.find(':') + 1, addr_.size() - 1).c_str());
                RPCServerMap[addr_] = std::make_shared<ButtonRPC>();
                RPCServerMap[addr_]->as_server(ip, port);
            }
        }
    }
    for (rapidxml::xml_node<>* book = root->first_node("RPCClient"); book; book = book->next_sibling("RPCClient"))
    {
        for (rapidxml::xml_node<>* addr_node = book->first_node("addr"); addr_node; addr_node = addr_node->next_sibling("addr"))
        {
            if (addr_node)
            {
                std::string addr_ = addr_node->value();
                std::string ip = addr_.substr(0, addr_.find(':'));
                int port = std::atoi(addr_.substr(addr_.find(':') + 1, addr_.size() - 1).c_str());
                RPCServerMap[addr_] = std::make_shared<ButtonRPC>();
                RPCServerMap[addr_]->as_client(ip, port);
            }
        }
    }
    // 遍历子元素
    for (rapidxml::xml_node<>* book = root->first_node("Agent"); book; book = book->next_sibling("Agent")) 
    {
        rapidxml::xml_node<>* name = book->first_node("name");
        if (name) 
        {
            std::string agentName = name->value();
            // 打印字符串
            spdlog::info("Get agents name {}", agentName);
            if (dllFuncLoaderPtr->openDll("Agents/" + agentName + "/" + agentName + ".dll")!= 0) 
            {
                spdlog::error("{} dll load fail.", agentName);
                continue;
            }
            else
            {
                spdlog::info("{} dll load success.", agentName);
            }

            AgentBase* (*GetAgent)();
            GetAgent = (AgentBase* (*)())(dllFuncLoaderPtr->getFunc("Agents/" + agentName + "/" + agentName + ".dll", "CreateAgent"));
            if (GetAgent ==NULL)
            {
                spdlog::info("{} CreateAgent function fail.", agentName);
                continue;
            }
            else
            {
                spdlog::info("{} CreateAgent function success.", agentName);
            }
            AgentBase* newAgent = (*GetAgent)();
            std::shared_ptr<AgentBase> agentPtr(newAgent);
            agentPtrVector.push_back(agentPtr);
            //设置定时函数，第一个值为毫秒数
            agentPtr->m_setTimerFunc= *setTimerFuncPtr;
            //获得定时函数
            //设置定时函数，第一个值为毫秒数
            agentPtr->m_getTimerFunc = *getTimerFuncPtr;
            //添加rpc函数,any为std::shared_ptr<std::Function>
            agentPtr->m_setRPCFunc= *setRPCFuncPtr;
            //获得rpc函数
            agentPtr->m_getRPCFunc= *getRPCFuncPtr;
            //添加dll函数,any为std::shared_ptr<std::Function>
            agentPtr->m_setDllFunc= *setDllFuncPtr;
            //获得dll函数
            agentPtr->m_getDllFunc= *getDllFuncPtr;
            ////添加消息
            agentPtr->m_pushMessageFunc= *pushMessagePtr;
            ////添加消息处理函数
            agentPtr->m_pushCallbackFunc = *pushCallbackFuncPtr;
            agentPtr->m_loggerPtr = loggerPtr;
            agentPtr->m_agentName = agentName;
            for (std::map<std::string, std::shared_ptr<ButtonRPC>>::iterator it_Client = RPCClientMap.begin(); it_Client != RPCClientMap.end(); it_Client++)
            {
                agentPtr->m_RPCClientMap[it_Client->first] = it_Client->second;
            }
            for (std::map<std::string, std::shared_ptr<ButtonRPC>>::iterator it_Server = RPCServerMap.begin(); it_Server != RPCServerMap.end(); it_Server++)
            {
                agentPtr->m_RPCServerMap[it_Server->first] = it_Server->second;
            }
            agentPtr->initialize();

        }

    }
    (*getTimerFuncPtr)(timerFuncMapPtr);
    std::vector<std::future<void>> runFuncVector;
    for (std::map<std::string, std::shared_ptr<ButtonRPC>>::iterator it = RPCServerMap.begin(); it != RPCServerMap.end(); it++)
    {
        runFuncVector.push_back(std::async(std::launch::async, std::bind(&ButtonRPC::run, it->second)));
    }

    for (std::vector<std::shared_ptr<AgentBase>>::iterator it = agentPtrVector.begin(); it != agentPtrVector.end(); it++)
    {
        std::function<void()> runFunc=std::bind(&AgentBase::run,*it);
        runFuncVector.push_back(std::async(std::launch::async, runFunc));

    }
    runFuncVector.push_back(std::async(std::launch::async, std::bind(&timer_run)));
    for (std::vector<std::future<void>>::iterator it = runFuncVector.begin(); it != runFuncVector.end(); it++)
    {
        it->get();
    }

	std::shared_ptr<TaskScheduler> taskSchedulerPtr = std::make_shared<TaskScheduler>(loggerPtr);
    taskSchedulerPtr->runTask();
    
    

}
