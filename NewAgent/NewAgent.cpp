#include <iostream>
#include <string>
#include <AgentBase.hpp>
#include <typeinfo>
class NewAgent : public AgentBase
{



public:

	NewAgent();
	virtual ~NewAgent();
	
	virtual void initialize();
	
	virtual void run();
	void timeout1();
	void timeout5();


private:
	int mm = 1;
};
GetAgent(NewAgent)
NewAgent::NewAgent()
{
}

NewAgent::~NewAgent()
{

}

void NewAgent::initialize()
{
	
	spdlog::set_default_logger(m_loggerPtr);
	
	spdlog::info("{} initialize", this->m_agentName);


}
void NewAgent::run()
{

	std::string topic1 = "topic1";
	std::string topic2 = "topic2";		
	publish<std::string>(topic1, std::string("11111"));
	publish<int>("topic2", 1);
	publish<int>("topic785", 74522);
	int aa = 2;
	subscribe<int>("topic2", aa);
	spdlog::info("{} subscribe", aa);

	std::function<void()> func1 =std::function<void()>(std::bind(&NewAgent::timeout1, this));

	std::function<void()> func5 = std::function<void()>(std::bind(&NewAgent::timeout5, this));
	std::string funcname5("func5");		
	m_addTimerFunc(UINT64(1), "func1",func1);
	try
	{

		//m_setTimerFunc(UINT64(5), funcname5, std::move(func5));
		spdlog::info("{} run", this->m_agentName);		


	}
	catch (std::exception& e)
	{
		spdlog::error("{} run fail", this->m_agentName);
	}
}
void NewAgent::timeout1()
{
	spdlog::info(" timeout {}", mm);
	mm++;
	if (mm==10000)
	{
		publish<int>("topic785", mm);
		spdlog::info("emmmmm");
		m_alterTimer(1000, "func1");
	}
}
void NewAgent::timeout5()
{
	spdlog::info("{} timeout 5", this->m_agentName);
}
