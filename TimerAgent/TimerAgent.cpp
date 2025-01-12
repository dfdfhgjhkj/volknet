#include <iostream>
#include <string>
#include <AgentBase.hpp>
#include <boost/asio.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/asio/io_service.hpp>
#include <boost/bind/bind.hpp>
class TimerAgent : public AgentBase
{
public:

	TimerAgent();
	virtual ~TimerAgent();
	//初始化
	virtual void initialize();
	//运行
	virtual void run();
	void timeout();
	//计时计数
	uint64_t m_count;
	boost::asio::io_context m_timer_ioc;
	//timer
	boost::asio::deadline_timer m_timer;
	//timerFuncMap
	std::shared_ptr<ThreadSafeMap<UINT64, std::map<std::string, std::shared_ptr<std::function<void()>>>>> timerFuncMapPtr;
private:
};
GetAgent(TimerAgent)
TimerAgent::TimerAgent():m_timer(boost::asio::deadline_timer(m_timer_ioc, boost::posix_time::microsec(1000)))
{
}

TimerAgent::~TimerAgent()
{

}

void TimerAgent::initialize()
{
	// 设置全局 logger
	spdlog::set_default_logger(m_loggerPtr);
	// 打印字符串
	spdlog::info("{} initialize", this->m_agentName);
	m_getTimerFunc(timerFuncMapPtr);
	

}
void TimerAgent::run()
{
	try
	{
		m_timer.async_wait(boost::bind(&TimerAgent::timeout, this));
		m_timer_ioc.run();
		spdlog::info("{} run", this->m_agentName);
	}
	catch (std::exception& e)
	{
		spdlog::error("{} run fail", this->m_agentName);
	}
}
void TimerAgent::timeout()
{
	m_timer.expires_at(m_timer.expires_at() + boost::posix_time::microsec(1000));
	for (ThreadSafeMap<UINT64, std::map<std::string, std::shared_ptr<std::function<void()>>>>::iterator it= timerFuncMapPtr->begin(); it!= timerFuncMapPtr->end(); it++)
	{
		if (m_count % it->first == 0)
		{		
			for (std::map<std::string, std::shared_ptr<std::function<void()>>>::iterator it1 = it->second.begin(); it1 != it->second.end(); it1++)
			{
				std::future<void> futureResult = std::async(std::launch::async, *it1->second);
				std::future_status status = futureResult.wait_for(std::chrono::nanoseconds(it->first*1000));
				switch (status)
				{
				case std::future_status::deferred:
					break;
				case std::future_status::timeout:
					//spdlog::error("{} function timeout, cycle time is {} ms.", it1->first, it->first);
					//超时
					break;
				case std::future_status::ready:
					//成功
					break;
				}
			}

		}

	}
	m_count++;
	m_timer.async_wait(boost::bind(&TimerAgent::timeout, this));
}
