
#include <AgentBase.hpp>
#include <ThreadSafeMap.hpp>
#include <Any>
#include <sstream>
#include <fstream>
#include <boost/interprocess/managed_shared_memory.hpp>
#include <boost/interprocess/containers/vector.hpp>
#include <boost/interprocess/containers/string.hpp>
#include <boost/interprocess/allocators/allocator.hpp>
#include <string>
#include <iostream>
#include <shared_mutex>
/*
这是第一个值得注意的点
当我们要在boost::interprocess中创建共享内存里的数据结构时
我们不能直接用std的分配器，必须要定义一下
而vector<string>,不仅vector需要定义分配器，string也需要定义分配器
然后把他俩套起来，才是正确的
*/
//using namespace boost::interprocess;
typedef  boost::interprocess::allocator<char, boost::interprocess::managed_shared_memory::segment_manager> CharAllocator;
typedef boost::interprocess::basic_string<char, std::char_traits<char>, CharAllocator> ShmemString;
typedef boost::interprocess::allocator<ShmemString, boost::interprocess::managed_shared_memory::segment_manager> StringAllocator;
typedef boost::interprocess::vector<ShmemString, StringAllocator> ShmemVector;


class person
{
public:
    //person() :name("") { age = 0; id = 0; };
    person(int id_ = 0, std::string name_ = "", int age_ = 0) :name(name_) { age = age_; id = id_; };

    int id;
    std::string name;
    int age;
	NLOHMANN_DEFINE_TYPE_INTRUSIVE(person, id, name, age);
};
class BlackBoardSaver
{
public:
    BlackBoardSaver()
    {

    }

    BlackBoardSaver(const BlackBoardSaver& bbs)
    {
        this->m_index=bbs.m_index;
        this->m_saveTime = bbs.m_saveTime;
        this->m_typename_ = bbs.m_typename_;
    }
    ~BlackBoardSaver()
    {

    }
    //更改时间
    std::time_t m_saveTime;
    //shm vector index
    UINT32 m_index;

    std::string m_typename_;


private:

};



class DataBaseAgent : public AgentBase
{



public:

    DataBaseAgent();
    virtual ~DataBaseAgent();
    //初始化
    virtual void initialize();
    //运行
    virtual void run();
private:
    //存储map
    std::shared_ptr<ThreadSafeMap<std::string, std::shared_ptr<BlackBoardSaver>>> m_saverMapPtr;
    //转字符串函数
    std::shared_ptr<ThreadSafeMap<std::string, std::shared_ptr<std::function<void(Serializer*, std::string&)>>>> m_toStringFuncMapPtr;
    //反转字符串函数
    std::shared_ptr<ThreadSafeMap<std::string, std::shared_ptr<std::function<void(Serializer*, std::string&)>>>> m_stringToFuncMapPtr;
    //发布接口
    bool publish1(std::string_view topic, Serializer& anyData);
    //RPC发布接口
    bool publish2(std::string topic, Serializer anyData);
    //订阅接口
    bool subscribe1(std::string_view topic, Serializer& anyData);    
    //RPC订阅接口
    bool subscribe2(std::string topic, Serializer anyData);


    //发布对应字符串
    bool publish_string1(std::string_view topic, std::string& value);    
    //发布对应字符串
    bool publish_string2(std::string& topic, std::string& value);
    //订阅对应字符串
    bool subscribe_string1(std::string_view topic, std::string& value);

    //订阅对应字符串
    bool subscribe_string2(std::string& topic, std::string& value);

    //获得所有的topic
    void getAllTopic(std::vector<std::string>& topicstdVector);
    //注册类型
    void subscribeType(const char* a, std::shared_ptr<std::function<void(Serializer*, std::string&)>> toStrFuncPtr, std::shared_ptr<std::function<void(Serializer*, std::string&)>> strToFuncPtr);
    std::shared_ptr<boost::interprocess::managed_shared_memory> m_segmentPtr;    
    std::shared_ptr <StringAllocator> m_strallocPtr;
    std::shared_ptr<ShmemVector> m_vectorPtr;
    std::shared_ptr <CharAllocator> m_charallocPtr;

    std::shared_timed_mutex m_mtx;

};
GetAgent(DataBaseAgent)
DataBaseAgent::DataBaseAgent()
{
}

DataBaseAgent::~DataBaseAgent()
{

}

void DataBaseAgent::initialize()
{
    // 设置全局 logger
    spdlog::set_default_logger(m_loggerPtr);    
    m_saverMapPtr = std::make_shared<ThreadSafeMap<std::string, std::shared_ptr<BlackBoardSaver>>>();
    m_toStringFuncMapPtr = std::make_shared<ThreadSafeMap<std::string, std::shared_ptr<std::function<void(Serializer*, std::string&)>>>>();
    m_stringToFuncMapPtr = std::make_shared<ThreadSafeMap<std::string, std::shared_ptr<std::function<void(Serializer*, std::string&)>>>>();
    subscribeType(RegisterType(int));
    subscribeType(RegisterType(double));
    subscribeType(RegisterType(std::string));
    subscribeType(RegisterType(std::vector<int>));
    subscribeType(RegisterType(std::vector<double>));
    subscribeType(RegisterType(std::vector<std::string>));
    subscribeType(RegisterType(person));



    boost::interprocess::shared_memory_object::remove("-1");

    m_segmentPtr =std::make_shared<boost::interprocess::managed_shared_memory>(boost::interprocess::create_only, "-1", 65536);

    m_strallocPtr =std::make_shared<StringAllocator>(m_segmentPtr->get_segment_manager());

    m_vectorPtr = std::shared_ptr<ShmemVector>(m_segmentPtr->find_or_construct<ShmemVector>("vec")(*m_strallocPtr));

    m_charallocPtr = std::make_shared<CharAllocator>(m_segmentPtr->get_segment_manager());
    std::any publishFunc = std::make_any<std::function<bool(std::string_view, Serializer &)>>(std::bind(&DataBaseAgent::publish1, this, std::placeholders::_1, std::placeholders::_2));
    std::any subscribeFunc = std::make_any<std::function<bool(std::string_view , Serializer & )>>(std::bind(&DataBaseAgent::subscribe1, this, std::placeholders::_1, std::placeholders::_2));
    std::any publishStringFunc = std::make_any<std::function<bool(std::string_view, std::string&)>>(std::bind(&DataBaseAgent::publish_string1, this, std::placeholders::_1, std::placeholders::_2));
    std::any subscribeStringFunc = std::make_any<std::function<bool(std::string_view, std::string&)>>(std::bind(&DataBaseAgent::subscribe_string1, this, std::placeholders::_1, std::placeholders::_2));
    m_setDllFunc("publish", publishFunc);
    m_setDllFunc("subscribe", subscribeFunc);
    m_setDllFunc("publish_string", publishStringFunc);
    m_setDllFunc("subscribe_string", subscribeStringFunc);

    std::function<int(const std::string&, std::string&)> RpcGetTopicFunc = [&](const std::string& in, std::string& out)
        {

            std::vector<std::string> topic_vector;
            getAllTopic(topic_vector);
            nlohmann::json j_vec(topic_vector);
            out = j_vec.dump();
            return 0;

        };

    m_setRPCFunc("publish_string", std::move(RpcGetTopicFunc));




    spdlog::info("{} initialize", this->m_agentName);
   
}
void DataBaseAgent::run()
{
    std::shared_ptr <person> personPtr= std::make_shared<person>((1, "tom", 20));

    spdlog::info("{} run", this->m_agentName);
}


bool DataBaseAgent::publish1(std::string_view topic, Serializer& anyData)
{
    std::string typename_;
    anyData >> typename_;
    if (m_toStringFuncMapPtr->find(typename_) != m_toStringFuncMapPtr->end())
    {

    }
    else
    {
        // 打印字符串
        spdlog::error("'Publish fail, register type:{} before publish.", typename_);
        return false;
    }


    if (m_saverMapPtr->find(topic.data()) != m_saverMapPtr->end())
    {
        {
            std::unique_lock<std::shared_timed_mutex> lck(m_mtx);
            (*m_vectorPtr)[(*m_saverMapPtr)[topic.data()]->m_index] = std::move(ShmemString(anyData.data(), anyData.size(), (*m_charallocPtr)));

        }
        (*m_saverMapPtr)[topic.data()]->m_saveTime = std::move(std::chrono::system_clock::to_time_t(std::chrono::system_clock::now()));
        (*m_saverMapPtr)[topic.data()]->m_typename_ = std::move(typename_);
    }
    else
    {
        std::shared_ptr<BlackBoardSaver> sbPtr = std::make_shared<BlackBoardSaver>();
        {

            std::unique_lock<std::shared_timed_mutex> lck(m_mtx);
            sbPtr->m_index = m_vectorPtr->size();
            m_vectorPtr->emplace_back(anyData.data(), anyData.size(), (*m_charallocPtr));

        }
        sbPtr->m_saveTime = std::move(std::chrono::system_clock::to_time_t(std::chrono::system_clock::now()));
        sbPtr->m_typename_ = std::move(typename_);
        m_saverMapPtr->insert(std::make_pair(topic.data(), sbPtr));
    }
    return true;
}
bool DataBaseAgent::publish2(std::string topic, Serializer anyData)
{
    std::string typename_;
    anyData >> typename_;
    if (m_toStringFuncMapPtr->find(typename_) != m_toStringFuncMapPtr->end())
    {

    }
    else
    {
        // 打印字符串
        spdlog::error("'Publish fail, register type:{} before publish.", typename_);
        return false;
    }


    if (m_saverMapPtr->find(topic.data()) != m_saverMapPtr->end())
    {

        {

            std::unique_lock<std::shared_timed_mutex> lck(m_mtx);
            (*m_vectorPtr)[(*m_saverMapPtr)[topic.data()]->m_index]= std::move(ShmemString(anyData.data(), anyData.size(), (*m_charallocPtr)));
        }        
        (*m_saverMapPtr)[topic.data()]->m_saveTime=std::move(std::chrono::system_clock::to_time_t(std::chrono::system_clock::now()));
        (*m_saverMapPtr)[topic.data()]->m_typename_ = std::move(typename_);
    }
    else
    {    
        std::shared_ptr<BlackBoardSaver> sbPtr = std::make_shared<BlackBoardSaver>();
        {

            std::unique_lock<std::shared_timed_mutex> lck(m_mtx);            
            sbPtr->m_index = m_vectorPtr->size();
            m_vectorPtr->emplace_back(anyData.data(), anyData.size(), (*m_charallocPtr));

        }        
        sbPtr->m_saveTime= std::move(std::chrono::system_clock::to_time_t(std::chrono::system_clock::now()));
        sbPtr->m_typename_ = std::move(typename_);
        m_saverMapPtr->insert(std::make_pair(topic.data(), sbPtr));
    }
    return true;
}
bool DataBaseAgent::subscribe1(std::string_view topic, Serializer& anyData)
{
    if (m_saverMapPtr->find(topic.data()) != m_saverMapPtr->end())
    {        
        anyData.clear();
        anyData << (*m_saverMapPtr)[topic.data()]->m_typename_;
        anyData.input((*m_vectorPtr)[(*m_saverMapPtr)[topic.data()]->m_index].data(), (*m_vectorPtr)[(*m_saverMapPtr)[topic.data()]->m_index].size());
        return true;
    }
    else
    {
        spdlog::error("'Subscribe fail, no topic: {}.", topic);
        return false;
    }

}

bool DataBaseAgent::subscribe2(std::string topic, Serializer anyData)
{
    if (m_saverMapPtr->find(topic.data()) != m_saverMapPtr->end())
    {
        anyData.clear();
        anyData << (*m_saverMapPtr)[topic.data()]->m_typename_;
        {
            std::shared_lock<std::shared_timed_mutex> lck(m_mtx);
            anyData.input((*m_vectorPtr)[(*m_saverMapPtr)[topic.data()]->m_index].data(), (*m_vectorPtr)[(*m_saverMapPtr)[topic.data()]->m_index].size());
        }
        return true;
    }
    else
    {
        spdlog::error("'Subscribe fail, no topic: {}.", topic);
        return false;
    }

}


//发布对应字符串
bool DataBaseAgent::publish_string1(std::string_view topic, std::string& value)
{
    if (m_saverMapPtr->find(topic.data()) != m_saverMapPtr->end())
    {
        std::shared_ptr<BlackBoardSaver> bbSaverPtr = (*m_saverMapPtr)[topic.data()];
        Serializer anyData;
        (*((*m_stringToFuncMapPtr)[bbSaverPtr->m_typename_]))(&anyData, value);
        return true;
    }
    else
    {
        spdlog::error("'Publish string fail, no topic: {}.", topic);
        return false;
    }

}
//订阅对应字符串
bool DataBaseAgent::subscribe_string1(std::string_view  topic, std::string& value)
{
    if (m_saverMapPtr->find(topic.data()) != m_saverMapPtr->end())
    {
        std::shared_ptr<BlackBoardSaver> bbSaverPtr = (*m_saverMapPtr)[topic.data()];
        Serializer anyData; 
        {
            std::shared_lock<std::shared_timed_mutex> lck(m_mtx);
            anyData.input((*m_vectorPtr)[(*m_saverMapPtr)[topic.data()]->m_index].data(), (*m_vectorPtr)[(*m_saverMapPtr)[topic.data()]->m_index].size());
        }
        (*((*m_toStringFuncMapPtr)[bbSaverPtr->m_typename_]))(&anyData, value);
        return true;
    }
    else
    {
        spdlog::error("'Subscribe string fail, no topic: {}.", topic);
        return false;
    }

}

//发布对应字符串
bool DataBaseAgent::publish_string2(std::string& topic, std::string& value)
{
    if (m_saverMapPtr->find(topic.data()) != m_saverMapPtr->end())
    {
        std::shared_ptr<BlackBoardSaver> bbSaverPtr = (*m_saverMapPtr)[topic.data()];
        Serializer anyData;
        (*((*m_stringToFuncMapPtr)[bbSaverPtr->m_typename_]))(&anyData, value);
        return true;
    }
    else
    {
        spdlog::error("'Publish string fail, no topic: {}.", topic);
        return false;
    }

}
//订阅对应字符串
bool DataBaseAgent::subscribe_string2(std::string&topic, std::string& value)
{
    if (m_saverMapPtr->find(topic.data()) != m_saverMapPtr->end())
    {
        std::shared_ptr<BlackBoardSaver> bbSaverPtr = (*m_saverMapPtr)[topic.data()];
        Serializer anyData;
        {
            std::shared_lock<std::shared_timed_mutex> lck(m_mtx);
            anyData.input((*m_vectorPtr)[(*m_saverMapPtr)[topic.data()]->m_index].data(), (*m_vectorPtr)[(*m_saverMapPtr)[topic.data()]->m_index].size());
        }
        (*((*m_toStringFuncMapPtr)[bbSaverPtr->m_typename_]))(&anyData, value);
        return true;
    }
    else
    {
        spdlog::error("'Subscribe string fail, no topic: {}.", topic);
        return false;
    }

}

//获得所有的topic
void DataBaseAgent::getAllTopic(std::vector<std::string>& topicstdVector)
{
    topicstdVector.clear();
    for (ThreadSafeMap<std::string, std::shared_ptr<BlackBoardSaver>>::iterator it = m_saverMapPtr->begin(); it !=m_saverMapPtr->end(); it++)
    {
        topicstdVector.push_back(it->first);
    }
}
void DataBaseAgent::subscribeType(const char* typename_, std::shared_ptr<std::function<void(Serializer*, std::string&)>> toStrFuncPtr, std::shared_ptr<std::function<void(Serializer*, std::string&)>> strToFuncPtr)
{
    (*m_toStringFuncMapPtr)[std::string(typename_)] = toStrFuncPtr;
    (*m_stringToFuncMapPtr)[std::string(typename_)] = strToFuncPtr;
    
}
