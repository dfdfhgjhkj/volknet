
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
    
    virtual void initialize();
    
    virtual void run();
private:

    
    void getAllTopic(std::vector<std::string>& topicstdVector);
    void getAllValue(std::vector<std::string>& valuestdVector);
    std::unordered_map<std::string, int64_t> m_bbHashMap;


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
            MapIterator iter;                
            std::vector<nlohmann::json> vjson;
            for (MapIterator iter = m_mapPtr->begin(); iter != m_mapPtr->end(); ++iter)
            {            
                nlohmann::json json_;
                std::string topic(iter->first.data(), iter->first.size());
                json_["topic"] = topic;
                Serializer SeData;
                m_subscribe(topic, SeData);
                std::string value;
                m_SeToJson(value, SeData);
                json_["value"] = value;
                vjson.push_back(json_);
                spdlog::info("{} dump", json_.dump());

            }
            nlohmann::json a(vjson);
            out = a.dump();
            return 0;

        };

    m_setRPCFunc("publish", std::move(RpcGetTopicFunc));

    std::function<int(const std::string&, std::string&)> RpcGetValueFunc = [&](const std::string& in, std::string& out)
        {
            Serializer SeData;
            m_subscribe(in, SeData);
            std::string jsonStr;
            m_SeToJson(jsonStr, SeData);
            nlohmann::json j_vec(jsonStr);
            out = j_vec.dump();
            return 0;

        };

    m_setRPCFunc("getValue", std::move(RpcGetValueFunc));


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
        nlohmann::json json_;
        json_["topic"] = std::string(iter->first.data(), iter->first.size());
        json_["value"] = std::string(iter->second.data(), iter->second.size());
        topicstdVector.push_back(json_.dump());
        spdlog::info("{} dump", json_.dump());

    }
    
}

void BlackBoardAgent::getAllValue(std::vector<std::string>& valuestdVector)
{
    MapIterator iter;
    for (MapIterator iter = m_mapPtr->begin(); iter != m_mapPtr->end(); ++iter)
    {
        valuestdVector.push_back(std::string(iter->second.data(), iter->second.size()));
    }

}


