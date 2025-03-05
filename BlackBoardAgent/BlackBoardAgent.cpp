
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

    std::shared_ptr<std::function<void(std::vector<std::string>&)>> m_getAllTopicFuncPtr;


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
    std::any topicFunc;
    m_getDllFunc("getAllTopic", topicFunc);
    try
    {
        m_getAllTopicFuncPtr = std::any_cast<std::shared_ptr<std::function<void(std::vector<std::string>&)>>>(topicFunc);
    }
    catch (const std::exception& e)
    {
        spdlog::error("{} error", e.what());
    }

    std::function<int(const std::string&, std::string&)> RpcGetTopicFunc = [&](const std::string& in, std::string& out)
        {

            std::vector<std::string> topic_vector;
            (*m_getAllTopicFuncPtr)(topic_vector);
            //MapIterator iter;                
            std::vector<nlohmann::json> vjson;
            for (auto iter = topic_vector.begin(); iter != topic_vector.end(); ++iter)
            {            
                nlohmann::json json_;
                json_["topic"] = *iter;
                Serializer SeData;
                spdlog::info("{} dump1", *iter);
                m_subscribe(iter->c_str(), SeData);
                std::string value;
                if (SeData.size()==0)
                {
                    continue;
                }
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



