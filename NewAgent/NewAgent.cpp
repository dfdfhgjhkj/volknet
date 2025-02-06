#include <iostream>
#include <string>
#include <AgentBase.hpp>
#include <typeinfo>
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
	std::function<bool(std::string_view, Serializer&)> publishFunc;
	std::function<bool(std::string_view, Serializer&)> subscribeFunc;

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
	m_getDllFunc("publish", publishFuncAny);
	std::any subscribeFuncAny;
	m_getDllFunc("subscribe", subscribeFuncAny);
	try
	{
		publishFunc = std::any_cast<std::function<bool(std::string_view, Serializer&)>>(publishFuncAny);
		subscribeFunc = std::any_cast<std::function<bool(std::string_view, Serializer&)>>(subscribeFuncAny);

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
	Serializer value1;
	value1 << typeid(std::string("11111")).name();
	value1 << std::string("11111");

	Serializer value2;
	value2 << typeid(1).name();
	value2 << 1;
	publishFunc(std::move(topic1), value1);
	publishFunc("topic2", value2);

	std::function<void()> func1 =std::function<void()>(std::bind(&NewAgent::timeout1, this));
	std::string funcname1("func1");

	std::function<void()> func5 = std::function<void()>(std::bind(&NewAgent::timeout5, this));
	std::string funcname5("func5");
	try
	{
		m_setTimerFunc(UINT64(1), "func1",std::move(func1));
		m_setTimerFunc(UINT64(5), funcname5, std::move(func5));
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
