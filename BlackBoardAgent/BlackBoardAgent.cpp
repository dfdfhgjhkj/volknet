
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
    RegisteType(int);
    RegisteType(double);
    RegisteType(std::string);
    RegisteType(std::vector<int>);
    RegisteType(std::vector<double>);
    RegisteType(std::vector<std::string>);
    RegisteType(person);



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

//获得所有的topic
void BlackBoardAgent::getAllTopic(std::vector<std::string>& topicstdVector)
{
    MapIterator iter;
    for (MapIterator iter = m_mapPtr->begin(); iter != m_mapPtr->end(); ++iter)
    {
        topicstdVector.push_back(std::string(iter->first.data(), iter->first.size()));
    }
    
}
