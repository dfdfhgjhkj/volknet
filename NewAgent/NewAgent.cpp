#include <iostream>
#include <string>
#include <AgentBase.hpp>
class NewAgent : public AgentBase
{



public:

	NewAgent();
	virtual ~NewAgent();
	//初始化
	virtual void initialize();
	//运行
	virtual void run();
	void timeout1();
	void timeout5();
	std::function<bool(const std::string&&, std::any&)> publishFunc;
	std::function<bool(const std::string&&, std::any&)> subscribeFunc;

private:
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
	// 设置全局 logger
	spdlog::set_default_logger(m_loggerPtr);
	// 打印字符串
	spdlog::info("{} initialize", this->m_agentName);
	std::any publishFuncAny;
	(*m_getDllFuncPtr)("publish", publishFuncAny);
	std::any subscribeFuncAny;
	(*m_getDllFuncPtr)("subscribe", subscribeFuncAny);
	try
	{
		publishFunc = std::any_cast<std::function<bool(const std::string&&, std::any&)>>(publishFuncAny);
		subscribeFunc = std::any_cast<std::function<bool(const std::string&&, std::any&)>>(subscribeFuncAny);

	}
	catch (const std::exception& e)
	{
		spdlog::error("any cast fail1{}", e.what());
		return;
	}

}
void NewAgent::run()
{

	std::string topic1 = "topic1";
	std::string topic2 = "topic2";
	try
	{
		std::any value1 = std::make_any<std::string>(std::string("11111"));
		std::any value2 = std::make_any<int>(7848);
		publishFunc(std::move(topic1), value1);
		publishFunc("topic2", value2);
	}	
	catch (const std::exception&e)
	{
		spdlog::error("any cast fail2{}", e.what());
		return;
	}



	std::function<void()> func1 =std::function<void()>(std::bind(&NewAgent::timeout1, this));
	std::string funcname1("func1");

	std::function<void()> func5 = std::function<void()>(std::bind(&NewAgent::timeout5, this));
	std::string funcname5("func5");
	try
	{
		(*m_setTimerFuncPtr)(UINT64(1), "func1",func1);
		(*m_setTimerFuncPtr)(UINT64(5), std::move(funcname5), func5);
		spdlog::info("{} run", this->m_agentName);

	}
	catch (std::exception& e)
	{
		spdlog::error("{} run fail", this->m_agentName);
	}
}
void NewAgent::timeout1()
{
	spdlog::info("{} timeout 1", this->m_agentName);
}
void NewAgent::timeout5()
{
	spdlog::info("{} timeout 5", this->m_agentName);
}