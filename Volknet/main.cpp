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
#include <ThreadSafeQueue.hpp>
#include <boost/interprocess/managed_shared_memory.hpp>
#include <boost/interprocess/containers/vector.hpp>
#include <boost/interprocess/containers/string.hpp>
#include <boost/interprocess/containers/map.hpp>
#include <boost/interprocess/allocators/allocator.hpp>
#include <boost/interprocess/sync/named_recursive_mutex.hpp>
//using namespace boost::interprocess;
typedef boost::interprocess::allocator<char, boost::interprocess::managed_shared_memory::segment_manager> CharAllocator;
typedef boost::interprocess::basic_string<char, std::char_traits<char>, CharAllocator> ShmemString;
typedef boost::interprocess::allocator<ShmemString, boost::interprocess::managed_shared_memory::segment_manager> StringAllocator;
typedef boost::interprocess::vector<ShmemString, StringAllocator> ShmemVector;
typedef std::pair<const ShmemString, ShmemString> MapKVType;
typedef boost::interprocess::allocator<MapKVType, boost::interprocess::managed_shared_memory::segment_manager> MapAllocator;
typedef boost::interprocess::map< ShmemString, ShmemString, std::less<ShmemString>, MapAllocator>  ShmemMap;
typedef boost::interprocess::map< ShmemString, ShmemString, std::less<ShmemString>, MapAllocator>::iterator MapIterator;
uint64_t g_count;
boost::asio::io_context g_timer_ioc;
//timer
boost::asio::deadline_timer timer(boost::asio::deadline_timer(g_timer_ioc, boost::posix_time::microsec(1000)));

std::shared_ptr<ThreadSafeMap<UINT64, std::map<std::string, std::shared_ptr<std::function<void()>>>>> g_timerFuncMapPtr;

std::shared_ptr <boost::interprocess::managed_shared_memory> g_segmentPtr;
std::shared_ptr <CharAllocator> g_charallocPtr;
std::shared_ptr <StringAllocator> g_strallocPtr;
std::shared_ptr <MapAllocator> g_mapallocPtr;
ShmemMap* g_mapPtr;
std::shared_ptr<boost::interprocess::named_recursive_mutex > g_namedMtxPtr;


std::map<std::string, ThreadSafeQueue<std::string>> g_nameQueueMap;

void timeout()
{
    timer.expires_at(timer.expires_at() + boost::posix_time::microsec(1000));
    for (ThreadSafeMap<UINT64, std::map<std::string, std::shared_ptr<std::function<void()>>>>::iterator it = g_timerFuncMapPtr->begin(); it != g_timerFuncMapPtr->end(); it++)
    {
        if (it->first==0)
        {
            continue;
        }
        if (g_count % it->first == 0)
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
    g_count++;
    timer.async_wait(boost::bind(&timeout));
}

void timer_run()
{
    try
    {
        timer.async_wait(boost::bind(&timeout));
        g_timer_ioc.run();

    }
    catch (std::exception& e)
    {
    }
}

Serializer publish_net(Serializer in)
{
    Serializer out;
    std::string name_;
    in >> name_;
    ShmemString nameSS(name_.data(), *g_charallocPtr);
    ShmemString dataSS(in.data(), in.size(), *g_charallocPtr);
    MapKVType pair_(nameSS, dataSS);

    g_namedMtxPtr->lock();
    g_mapPtr->insert_or_assign(nameSS, dataSS);
    g_namedMtxPtr->unlock();

    out << 0;
    return out;
}

Serializer subscribe_net(Serializer in)
{

    std::string nameStr;
    in >> nameStr;
    ShmemString dataSS(*g_charallocPtr);
    ShmemString nameSS(nameStr.data(), *g_charallocPtr);

    if (g_mapPtr->find(nameSS) == g_mapPtr->end())
    {
        return Serializer();
    }
    {
        g_namedMtxPtr->lock();
        dataSS = g_mapPtr->at(nameSS);
        g_namedMtxPtr->unlock();
    }
    Serializer out(dataSS.data(), dataSS.size());
    return out;
}

int main()
{
    std::shared_ptr<spdlog::details::thread_pool> tp = std::make_shared<spdlog::details::thread_pool>(1,1);
   
    std::shared_ptr<spdlog::sinks::rotating_file_sink<std::mutex>> file_sink = std::make_shared<spdlog::sinks::rotating_file_sink_mt>("logs/logfile.log", 500 * 1024 * 1024, 1000);
   
    std::shared_ptr<spdlog::sinks::stdout_color_sink_mt> console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
  
    std::vector<spdlog::sink_ptr> sinks{ console_sink, file_sink };
    std::shared_ptr<spdlog::async_logger> loggerPtr = std::make_shared<spdlog::async_logger>("multi_sink", sinks.begin(), sinks.end(),tp);

  
    spdlog::set_default_logger(loggerPtr);
   
    spdlog::set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%^%l%$] [thread %t] %v");

   
    spdlog::set_level(spdlog::level::debug);

  
    spdlog::flush_on(spdlog::level::info);

    spdlog::info("VOLKNET online.");
    spdlog::info("Establishing battlefield control, standby.");
    spdlog::info("battlefield control online.");    
    g_segmentPtr = std::make_shared<boost::interprocess::managed_shared_memory>(boost::interprocess::open_or_create, "-1", 65536);
    g_charallocPtr = std::make_shared<CharAllocator>(g_segmentPtr->get_segment_manager());
    g_strallocPtr = std::make_shared<StringAllocator>(g_segmentPtr->get_segment_manager());
    g_mapallocPtr = std::make_shared<MapAllocator>(g_segmentPtr->get_segment_manager());
    g_mapPtr = g_segmentPtr->find_or_construct<ShmemMap>("map")(*g_mapallocPtr);

    g_namedMtxPtr = std::make_shared<boost::interprocess::named_recursive_mutex>(boost::interprocess::open_or_create, "map");
    std::shared_ptr<TaskScheduler> taskScheduler = std::make_shared<TaskScheduler>(loggerPtr);

    std::shared_ptr<std::function<int(UINT64 num,std::string_view name,  std::function<void()>)>> 
        setTimerFuncPtr=
        std::make_shared<std::function<int(UINT64 num,std::string_view name,  std::function<void()>)>>
        (std::bind(&TaskScheduler::SetTimerFunc,taskScheduler,std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
    std::shared_ptr<std::function<int(UINT64 num, std::string_view name)>>
        getTimerFuncPtr =
        std::make_shared<std::function<int(UINT64 num, std::string_view name)>>
        (std::bind(&TaskScheduler::SetFuncTime, taskScheduler, std::placeholders::_1, std::placeholders::_2));

    std::shared_ptr<std::function<int(std::string_view funcName, std::function<int(const std::string&, std::string&)>)>>
        setRPCFuncPtr=
        std::make_shared<std::function<int(std::string_view funcName, std::function<int(const std::string&, std::string&)>)>>
        (std::bind(&TaskScheduler::SetRPCFunc, taskScheduler, std::placeholders::_1, std::placeholders::_2));
    
    std::shared_ptr<std::function<int(std::string_view funcName, std::function<int(const std::string&, std::string&)>&)>>
        getRPCFuncPtr=
        std::make_shared<std::function<int(std::string_view funcName, std::function<int(const std::string&, std::string&)>&)>>
        (std::bind(&TaskScheduler::GetRPCFunc, taskScheduler, std::placeholders::_1, std::placeholders::_2));
   
    std::shared_ptr<std::function<int(std::string_view funcName, std::any)>>  
        setDllFuncPtr=
        std::make_shared<std::function<int(std::string_view funcName, std::any)>>
        (std::bind(&TaskScheduler::SetDllFunc, taskScheduler, std::placeholders::_1, std::placeholders::_2));
    
    std::shared_ptr<std::function<int(std::string_view funcName, std::any&)>>  
        getDllFuncPtr=
        std::make_shared<std::function<int(std::string_view funcName, std::any&)>>
        (std::bind(&TaskScheduler::GetDllFunc, taskScheduler, std::placeholders::_1, std::placeholders::_2));
    
    std::shared_ptr<std::function<int(std::string_view queueName, std::shared_ptr<MessageBase>)>>
        pushMessagePtr =
        std::make_shared<std::function<int(std::string_view queueName, std::shared_ptr<MessageBase>)>>
        (std::bind(&TaskScheduler::AddMessage, taskScheduler, std::placeholders::_1, std::placeholders::_2));
  
    std::shared_ptr<std::function<int(std::string_view, int, std::string_view, std::function<void(std::shared_ptr<MessageBase>)>)>>  
        pushCallbackFuncPtr=
        std::make_shared<std::function<int(std::string_view,int, std::string_view, std::function<void(std::shared_ptr<MessageBase>)>)>>
        (std::bind(&TaskScheduler::SetAsyncFunc, taskScheduler, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3, std::placeholders::_4));

    spdlog::flush_every(std::chrono::seconds(5)); 

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
  
    doc.parse<rapidxml::parse_default>(&(*xml_contents)[0]);
 
    rapidxml::xml_node<>* root = doc.first_node("Agents");

    std::vector<std::shared_ptr<AgentBase>> agentPtrVector;

    std::shared_ptr<DllFuncLoader> dllFuncLoaderPtr= std::make_shared<DllFuncLoader>();

    std::map<std::string, std::shared_ptr<ButtonRPC>> RPCServerMap;
    std::map<std::string, std::shared_ptr<ButtonRPC>> RPCClientMap;

    std::shared_ptr<ThreadSafeMap<size_t, std::string>> typenameMapPtr=std::make_shared<ThreadSafeMap<size_t, std::string>>();

    std::shared_ptr<ThreadSafeMap<size_t, std::pair<std::function<void(std::string& jsonStr, Serializer& SeData )>, std::function<void(std::string& jsonStr, Serializer& SeData)>>>> typeToStrFuncMapPtr=
        std::make_shared<ThreadSafeMap<size_t, std::pair<std::function<void(std::string& jsonStr, Serializer& SeData)>, std::function<void(std::string& jsonStr, Serializer& SeData)>>>>();
    for (rapidxml::xml_node<>* book = root->first_node("RPCServer"); book; book = book->next_sibling("RPCServer"))
    {
        for (rapidxml::xml_node<>* addr_node = book->first_node("addr"); addr_node; addr_node = addr_node->next_sibling("addr"))
        {
            if (addr_node)
            {
                std::string addr_ = addr_node->value();
                std::string ip = addr_.substr(0, addr_.find(':'));
                int port = std::atoi(addr_.substr(addr_.find(':') + 1, addr_.size() - 1).c_str());
                if (ip=="shm")
                {
                    RPCServerMap["shm"] = std::make_shared<ButtonRPC>();
                    RPCServerMap["shm"]->as_server(ip, port);

                }
                else
                {
                    RPCServerMap["net"] = std::make_shared<ButtonRPC>();
                    RPCServerMap["net"]->as_server(ip, port);                    
                    RPCServerMap["net"]->regist("subscribe_net", &subscribe_net);
                    RPCServerMap["net"]->regist("publish_net", &publish_net);
                }

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
    std::shared_ptr<std::function<void(std::string_view)>> addNameQueueFuncPtr = std::make_shared<std::function<void(std::string_view)>>(
        [&](std::string_view name)
        {
            g_nameQueueMap.insert(std::make_pair(std::string(name), ThreadSafeQueue<std::string>()) );
        }
    );
    std::shared_ptr<std::function<void(std::vector<std::string>&)>> getAllTopicFuncPtr = std::make_shared<std::function<void(std::vector<std::string>&)>>(
        [&](std::vector<std::string>& topicVector)
        {
            MapIterator iter;
            for (MapIterator iter = g_mapPtr->begin(); iter != g_mapPtr->end(); ++iter)
            {
                topicVector.push_back(std::string(iter->first.data(), iter->first.size()));
            }
        }
    );

    (*setDllFuncPtr)("getAllTopic",std::make_any<std::shared_ptr<std::function<void(std::vector<std::string>&)>>>(getAllTopicFuncPtr));
    std::shared_ptr<std::function<void(size_t a, const char* b, std::function<void(std::string& jsonStr, Serializer& SeData)>&& toStrFunc, std::function<void(std::string& jsonStr, Serializer& SeData)>&& strToFunc)>> registeTypeFuncPtr =
        std::make_shared<std::function<void(size_t a, const char* b, std::function<void(std::string & jsonStr, Serializer & SeData)> && toStrFunc, std::function<void(std::string & jsonStr, Serializer & SeData)> && strToFunc)>>(
        [&](size_t a, const char* b, std::function<void(std::string& jsonStr, Serializer& SeData)>&& toStrFunc, std::function<void(std::string& jsonStr, Serializer& SeData)>&& strToFunc)
        {
            if (typeToStrFuncMapPtr->find(a) != typeToStrFuncMapPtr->end())
            {
                spdlog::error("typename {} have been Registed", b);
                return;
            }
            if (typenameMapPtr->find(a) != typenameMapPtr->end())
            {
                spdlog::error("typename {} hash collisions with typename{}", b, (*typenameMapPtr)[a]);
                return;
            }
            std::shared_ptr<std::function<std::string(Serializer SeData)>> ToStrFuncPtr =
                std::make_shared<std::function<std::string(Serializer  SeData)>>
                (
                    [&](Serializer SeData)
                    {
                        std::string JsonStr;
                        toStrFunc(JsonStr, SeData);
                        return JsonStr;
                    }
                );
            std::shared_ptr<std::function<Serializer(std::string JsonStr)>> StrToFuncPtr =
                std::make_shared<std::function<Serializer(std::string JsonStr)>>
                (
                    [&](std::string JsonStr)
                    {
                        Serializer SeData;
                        toStrFunc(JsonStr, SeData);
                        return SeData;
                    }
                );
            typeToStrFuncMapPtr->insert(std::pair(a, std::make_pair(toStrFunc, strToFunc)));
            typenameMapPtr->insert(std::pair(a, b));
            RPCServerMap["net"]->regist(std::to_string(a) + "ToStr", *ToStrFuncPtr);

            RPCServerMap["shm"]->regist(std::to_string(a) + "ToStr", *ToStrFuncPtr);
            RPCServerMap["net"]->regist(std::to_string(a) + "ToSe", *StrToFuncPtr);
            RPCServerMap["shm"]->regist(std::to_string(a) + "ToSe", *StrToFuncPtr);
                
            return;
        });

    std::shared_ptr<std::function<void(std::string_view name, Serializer& dataSe)>> subscribeFuncPtr
        = std::make_shared<std::function<void(std::string_view name, Serializer & dataSe)>>(
            [&](std::string_view name, Serializer& dataSe)
            {
                ShmemString nameSS(name, *g_charallocPtr);

                if (g_mapPtr->find(nameSS) == g_mapPtr->end())
                {

                    for (std::map<std::string, std::shared_ptr<ButtonRPC>>::iterator it = RPCClientMap.begin(); it != RPCClientMap.end(); it++)
                    {
                        std::string ip = it->first.substr(0, it->first.find(':'));
                        if (ip != "shm")
                        {
                            dataSe = it->second->call<Serializer>("subscribe_net", std::string(name)).value();
                            return;
                        }
                        if (dataSe.size() == 0)
                        {
                            return;
                        }
                    }
                }
                else
                {
                    ShmemString dataSS(*g_charallocPtr);
                    g_namedMtxPtr->lock();
                    dataSS = g_mapPtr->at(nameSS);
                    g_namedMtxPtr->unlock();

                    dataSe.input(dataSS.data(), dataSS.size());
                }
                return;
            }
        );
    std::shared_ptr<std::function<void(std::string_view name, Serializer& dataSe)>> publishFuncPtr
        = std::make_shared<std::function<void(std::string_view name, Serializer & dataSe)>>(
            [&](std::string_view name, Serializer& dataSe)
            {
                for (auto it = g_nameQueueMap.begin(); it !=g_nameQueueMap.end(); it++)
                {
                    std::string Topic = std::string(name);
                    it->second.push(Topic);
                }
                ShmemString nameSS(name, *g_charallocPtr);
                ShmemString dataSS(dataSe.data(), dataSe.size(), *g_charallocPtr);
                MapKVType pair_(nameSS, dataSS);

                g_namedMtxPtr->lock();
                g_mapPtr->insert_or_assign(nameSS, dataSS);
                g_namedMtxPtr->unlock();

                for (std::map<std::string, std::shared_ptr<ButtonRPC>>::iterator it = RPCClientMap.begin(); it != RPCClientMap.end(); it++)
                {
                    std::string ip = it->first.substr(0, it->first.find(':'));
                    if (ip != "shm")
                    {
                        it->second->call<Serializer>("publish_net", dataSe);
                    }

                }
                return;
            }
        );
    
    //SeToJsonFunc
    std::shared_ptr<std::function<void(std::string& jsonStr, Serializer& dataSe)>> SeToJsonFuncPtr=
        std::make_shared<std::function<void(std::string& jsonStr, Serializer & dataSe)>>(
            [&](std::string& jsonStr, Serializer& dataSe)
            {
                size_t a;
                dataSe >> a;
                if (typeToStrFuncMapPtr->find(a) == typeToStrFuncMapPtr->end())
                {
                    spdlog::error("typename not Registed");
                    return;
                }
                (*typeToStrFuncMapPtr)[a].first(jsonStr,dataSe);
            }
        
        );
    //JsonToSeFunc
    std::shared_ptr<std::function<void(std::string& jsonStr, Serializer& dataSe)>> JsonToSeFuncPtr =
        std::make_shared<std::function<void(std::string & jsonStr, Serializer & dataSe)>>(
            [&](std::string& jsonStr, Serializer& dataSe)
            {
                nlohmann::json json_ = nlohmann::json::parse(jsonStr);
                size_t a = json_["typeHash"].get<size_t>();

                if (typeToStrFuncMapPtr->find(a) == typeToStrFuncMapPtr->end())
                {
                    spdlog::error("typename not Registed");
                    return;
                }                
                json_.erase("typeHash"); 
                std::string jsonStr_ = json_.dump();
                (*typeToStrFuncMapPtr)[a].second(jsonStr_, dataSe);
            }

        );
  

    for (rapidxml::xml_node<>* book = root->first_node("Agent"); book; book = book->next_sibling("Agent")) 
    {
        rapidxml::xml_node<>* name = book->first_node("name");
        if (name) 
        {
            std::string agentName = name->value();
     
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

            agentPtr->m_addTimerFunc = *setTimerFuncPtr;
         
            agentPtr->m_alterTimer = *getTimerFuncPtr;
       
            agentPtr->m_setRPCFunc= *setRPCFuncPtr;
          
            agentPtr->m_getRPCFunc= *getRPCFuncPtr;
        
            agentPtr->m_setDllFunc= *setDllFuncPtr;
           
            agentPtr->m_getDllFunc= *getDllFuncPtr;
         
            agentPtr->m_pushMessage= *pushMessagePtr;
    
            agentPtr->m_pushCallBackFunc = *pushCallbackFuncPtr;
            agentPtr->m_typenameMapPtr = typenameMapPtr;

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
            agentPtr->m_registeType = *registeTypeFuncPtr;
            agentPtr->m_subscribe = *subscribeFuncPtr;
            agentPtr->m_publish = *publishFuncPtr;
            agentPtr->m_SeToJson = *SeToJsonFuncPtr;
            agentPtr->m_JsonToSe= *JsonToSeFuncPtr;

            agentPtr->m_mapPtr = g_mapPtr;
            agentPtr->initialize();

        }

    }
    g_timerFuncMapPtr = taskScheduler->m_timerFunctionMapPtr;
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
