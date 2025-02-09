#include <AgentBase.hpp>
#include <ThreadSafeMap.hpp>
#include <sstream>
#include <fstream>

#include <string>
#include <iostream>
#include <shared_mutex>

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



class BlackBoardAgent : public AgentBase
{



public:

    BlackBoardAgent();
    virtual ~BlackBoardAgent();
    //初始化
    virtual void initialize();
    //运行
    virtual void run();
private:

    //获得所有的topic
    void getAllTopic(std::vector<std::string>& topicstdVector);



};
GetAgent(BlackBoardAgent)
BlackBoardAgent::BlackBoardAgent()
{
}

BlackBoardAgent::~BlackBoardAgent()
{

}

void BlackBoardAgent::initialize()
{
    // 设置全局 logger
    spdlog::set_default_logger(m_loggerPtr);    
    RegisterType(int);
    RegisterType(double);
    RegisterType(std::string);
    RegisterType(std::vector<int>);
    RegisterType(std::vector<double>);
    RegisterType(std::vector<std::string>);
    RegisterType(person);



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
void BlackBoardAgent::run()
{
    std::shared_ptr <person> personPtr= std::make_shared<person>((1, "tom", 20));

    spdlog::info("{} run", this->m_agentName);
}


void BlackBoardAgent::getAllTopic(std::vector<std::string>& topicstdVector)
{
    MapIterator iter;
    for (MapIterator iter = m_mapPtr->begin(); iter != m_mapPtr->end(); ++iter)
    {
        topicstdVector.push_back(std::string(iter->first.data(), iter->first.size()));
    }
}
