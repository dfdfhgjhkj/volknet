#include <iostream>
#include <AgentBase.hpp>
#include <ThreadSafeMap.hpp>
#include <Any>
#include <sstream>



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
    ~BlackBoardSaver()
    {

    }
    BlackBoardSaver(std::any& any)
    {
        m_any = any;

        // 将时间点转换为time_t以便格式化
        m_saveTime = std::move(std::chrono::system_clock::to_time_t(std::chrono::system_clock::now()));
    }
    //更改时间
    std::time_t m_saveTime;
    //指针
    std::any m_any;


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
    std::shared_ptr<ThreadSafeMap<std::string, std::shared_ptr<std::function<void(std::any&, std::string&)>>>> m_toStringFuncMapPtr;
    //反转字符串函数
    std::shared_ptr<ThreadSafeMap<std::string, std::shared_ptr<std::function<void(std::any&, std::string&)>>>> m_stringToFuncMapPtr;
    //发布接口
    bool publish(const std::string&& topic,std::any& any_ptr);
    //订阅接口
    bool subscribe(const std::string&& topic, std::any& any_ptr);
    //发布对应字符串
    bool publish_string(const std::string&& topic, std::string& value);
    //订阅对应字符串
    bool subscribe_string(const std::string&& topic, std::string& value);
    //获得所有的topic
    void getAllTopic(std::vector<std::string>& topicstdVector);
    //注册类型
    void subscribeType(const char* a, std::shared_ptr<std::function<void(std::any&, std::string&)>> toStrFuncPtr, std::shared_ptr<std::function<void(std::any&, std::string&)>> strToFuncPtr);
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
    m_toStringFuncMapPtr = std::make_shared<ThreadSafeMap<std::string, std::shared_ptr<std::function<void(std::any&, std::string&)>>>>();
    m_stringToFuncMapPtr = std::make_shared<ThreadSafeMap<std::string, std::shared_ptr<std::function<void(std::any&, std::string&)>>>>();
    subscribeType(RegisterType(int));
    subscribeType(RegisterType(double));
    subscribeType(RegisterType(std::string));
    subscribeType(RegisterType(std::vector<int>));
    subscribeType(RegisterType(std::vector<double>));
    subscribeType(RegisterType(std::vector<std::string>));
    subscribeType(RegisterType(person));


    std::any publishFunc=std::make_any<std::function<bool(const std::string&&, std::any&)>>(std::bind( & DataBaseAgent::publish, this, std::placeholders::_1, std::placeholders::_2));
    std::any subscribeFunc = std::make_any<std::function<bool(const std::string&&, std::any&)>>(std::bind(&DataBaseAgent::subscribe, this, std::placeholders::_1, std::placeholders::_2));
    std::any publishStringFunc = std::make_any<std::function<bool(const std::string&&, std::string&)>>(std::bind(&DataBaseAgent::publish_string, this, std::placeholders::_1, std::placeholders::_2));
    std::any subscribeStringFunc = std::make_any<std::function<bool(const std::string&&, std::string&)>>(std::bind(&DataBaseAgent::subscribe_string, this, std::placeholders::_1, std::placeholders::_2));
    (*m_setDllFuncPtr)("publish", publishFunc);
    (*m_setDllFuncPtr)("subscribe", subscribeFunc);
    (*m_setDllFuncPtr)("publish_string", publishStringFunc);
    (*m_setDllFuncPtr)("subscribe_string", subscribeStringFunc);

    std::function<int(const std::string&, std::string&)> RpcGetTopicFunc = [&](const std::string& in, std::string&out)
        {
        
            std::vector<std::string> topic_vector;    
            getAllTopic(topic_vector);
            nlohmann::json j_vec(topic_vector);
            out = j_vec.dump();
            return 0;

        };

    (*m_setRPCFuncPtr)("publish_string", RpcGetTopicFunc);

    // 打印字符串
    spdlog::info("{} initialize", this->m_agentName);
   
}
void DataBaseAgent::run()
{
    std::shared_ptr <person> personPtr= std::make_shared<person>((1, "tom", 20));
    // 打印字符串
    spdlog::info("{} run", this->m_agentName);
}
bool DataBaseAgent::publish(const std::string&& topic,std::any& anyData)
{
    if (m_toStringFuncMapPtr->find(anyData.type().name())!=m_toStringFuncMapPtr->end())
    {

    }
    else
    {
        // 打印字符串
        spdlog::error("'Publish fail, register type:{} before publish.", anyData.type().name());
        return false;
    }
    if (m_saverMapPtr->find(topic)!= m_saverMapPtr->end())
    {        
        (*m_saverMapPtr)[topic] = std::make_shared<BlackBoardSaver>(anyData);
    }
    else
    {
;        m_saverMapPtr->insert(std::make_pair(topic, std::make_shared<BlackBoardSaver>(anyData)));
    }
    return true;

}
bool DataBaseAgent::subscribe(const std::string&& topic, std::any& anyData)
{
    if (m_saverMapPtr->find(topic) != m_saverMapPtr->end())
    {        
        anyData = (*m_saverMapPtr)[topic]->m_any;
        return true;
    }
    else
    {
        spdlog::error("'Subscribe fail, no topic: {}.", topic);
        return false;
    }

}

//发布对应字符串
bool DataBaseAgent::publish_string(const std::string&& topic, std::string& value)
{
    if (m_saverMapPtr->find(topic) != m_saverMapPtr->end())
    {
        std::shared_ptr<BlackBoardSaver> bbSaverPtr = (*m_saverMapPtr)[topic];
        (*((*m_stringToFuncMapPtr)[bbSaverPtr->m_any.type().name()]))(bbSaverPtr->m_any, value);
        return true;
    }
    else
    {
        spdlog::error("'Publish string fail, no topic: {}.", topic);
        return false;
    }

}
//订阅对应字符串
bool DataBaseAgent::subscribe_string(const std::string&& topic, std::string& value)
{
    if (m_saverMapPtr->find(topic) != m_saverMapPtr->end())
    {
        std::shared_ptr<BlackBoardSaver> bbSaverPtr = (*m_saverMapPtr)[topic];
        (*((*m_toStringFuncMapPtr)[bbSaverPtr->m_any.type().name()]))(bbSaverPtr->m_any, value);
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
void DataBaseAgent::subscribeType(const char* typename_, std::shared_ptr<std::function<void(std::any&, std::string&)>> toStrFuncPtr, std::shared_ptr<std::function<void(std::any&, std::string&)>> strToFuncPtr)
{
    (*m_toStringFuncMapPtr)[std::string(typename_)] = toStrFuncPtr;
    (*m_stringToFuncMapPtr)[std::string(typename_)] = strToFuncPtr;
    
}