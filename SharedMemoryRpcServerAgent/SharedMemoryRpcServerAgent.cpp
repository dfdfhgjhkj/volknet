#include <iostream>
#include <boost/interprocess/managed_shared_memory.hpp>
#include <boost/interprocess/allocators/allocator.hpp>
#include <boost/interprocess/containers/string.hpp>
#include <boost/lockfree/queue.hpp>
#include <string>
#include <AgentBase.hpp>

uint32_t getCurrentProcessId() {
#ifdef _WIN32
	return GetCurrentProcessId();
#else
	return ::getpid();
#endif
}

//首先定义共享内存分配器类型
typedef boost::interprocess::allocator<char, boost::interprocess::managed_shared_memory::segment_manager> CharAllocator;
typedef boost::interprocess::basic_string<char, std::char_traits<char>, CharAllocator> shared_string;


//用于IPCHandle传输
class IPCHandleData {
public:
	IPCHandleData(const shared_string::allocator_type& al) :m_rpc(al)
	{

	}
	~IPCHandleData()
	{

	}
	shared_string m_rpc;//rpc数据
	uint32_t m_clientPid;//客户端pid
	uint32_t m_state;//状态0:没处理1:处理了

};

//定义共享内存handle类型
typedef boost::interprocess::managed_shared_memory::handle_t IPCHandleT;

//定义该handle的共享内存分配器
typedef boost::interprocess::allocator<IPCHandleT,
	boost::interprocess::managed_shared_memory::segment_manager>
	MyIPCHandleAllocatorT;

//定义基于该IPCHandle以及对应的内存分配器的Queue
typedef boost::lockfree::queue<IPCHandleT,
	boost::lockfree::capacity<65534>,  //最多时2^16-2
	boost::lockfree::allocator<MyIPCHandleAllocatorT>>
	IPCHandleMwMrQueueT;
IPCHandleMwMrQueueT;


void Creator()
{
	// 删除遗留的共享内存
	boost::interprocess::shared_memory_object::remove("MySharedMemory");
	// 构建新的托管共享内存区
	boost::interprocess::managed_shared_memory segment(boost::interprocess::create_only,
		"MySharedMemory",  //segment name
		65536 * 1024 * 10);
	// 定义托管共享内存区的分配器
	const MyIPCHandleAllocatorT alloc_inst(segment.get_segment_manager());
	try
	{
		// 创建共享内存中的用于传递IPCHandleT的无锁队列,放请求
		IPCHandleMwMrQueueT* pMyIPCHandleQueueRequest =segment.construct<IPCHandleMwMrQueueT>("IPCHandleQueueRequest")(alloc_inst);
		// 创建共享内存中的用于传递IPCHandleT的无锁队列,放结果
		IPCHandleMwMrQueueT* pMyIPCHandleQueueResult = segment.construct<IPCHandleMwMrQueueT>("IPCHandleQueueResult")(alloc_inst);
	}
	catch (std::exception& e)
	{
		std::cout << e.what() << std::endl;
	}
}


void Producer()
{

	//附接目标共享内存
	boost::interprocess::managed_shared_memory segment(boost::interprocess::open_only,
		"MySharedMemory");  //segment name

	typedef std::pair<IPCHandleMwMrQueueT*,
		boost::interprocess::managed_shared_memory::size_type> ResultT;
	ResultT res = segment.find<IPCHandleMwMrQueueT>("IPCHandleQueueRequest");

	if (!res.second)
	{
		return;
	}
	else
	{
	}

	IPCHandleMwMrQueueT* pMyIPCHandleQueue = res.first;

	int id = 1;

	//创建匿名共享内存对象，销毁时需要用destroy_ptr
	IPCHandleData* pIPCHandleData =
		segment.construct<IPCHandleData>
		(boost::interprocess::anonymous_instance)
		(shared_string::allocator_type(segment.get_segment_manager()));
	/*
	uint32_t m_clientPid;//客户端pid
	int m_state;//状态0:no call 1:call already*/
	pIPCHandleData->m_clientPid = getCurrentProcessId();
	std::stringstream ss1;
	pIPCHandleData->m_rpc = ss1.str().c_str();
	pIPCHandleData->m_state =0;

	//获取共享内存对应的handle
	IPCHandleT curHandle = segment.get_handle_from_address(pIPCHandleData);

	//将该handle放入queue中，传递给对端
	pMyIPCHandleQueue->push(curHandle);

}


void Consumer(AgentBase* agentPtr)
{
	//附接目标共享内存
	boost::interprocess::managed_shared_memory segment(boost::interprocess::open_only,
		"MySharedMemory");  //segment name

	typedef std::pair<IPCHandleMwMrQueueT*,
		boost::interprocess::managed_shared_memory::size_type> ResultT;
	ResultT res1 = segment.find<IPCHandleMwMrQueueT>("IPCHandleQueueRequest");
	ResultT res2 = segment.find<IPCHandleMwMrQueueT>("IPCHandleQueueResult");
	//std::cout << "res.second=" << res.second << std::endl;
	if (!res1.second||!res2.second)
	{
		//std::cout << "could not find \"MyIPCHandleQueue\"" << std::endl;
		return;
	}
	else
	{
	}

	IPCHandleMwMrQueueT* pMyIPCHandleQueue = res1.first;
	while (1)
	{
		IPCHandleT requestHandle;
		if (pMyIPCHandleQueue->pop(requestHandle))
		{

			//接收到消息后，通过handle找到对应的共享内存对象
			IPCHandleData* pIPCHandleData =
				(IPCHandleData*)segment.get_address_from_handle(requestHandle);
			std::string outStr;
			std::ostringstream ss;
			ss << pIPCHandleData->m_rpc;

			outStr = ss.str();

			nlohmann::json rpcDataJson = nlohmann::json::parse(outStr);
			std::string funcName(std::move(rpcDataJson["name"].get<std::string>()));
			std::string funcArg(std::move(rpcDataJson["arg"].get<std::string>()));
			std::string funcRes;
			std::function<int(const std::string&, std::string&)> rpcFunc;
			//默认成功调用
			int resCode = 1;

			if (agentPtr->m_getRPCFunc(std::move(rpcDataJson["name"].get<std::string>()), rpcFunc) != 0)
			{
				resCode = 2;
			}
			else
			{
				//检测是否超时
				rpcFunc(funcArg, funcRes);

			}
			//将该handle放入queue中，传递给对端
			res2.first->push(requestHandle);
		}

	}

}


class SharedMemoryRpcAgent : public AgentBase
{



public:

	SharedMemoryRpcAgent();
	virtual ~SharedMemoryRpcAgent();
	//初始化
	virtual void initialize();
	//运行
	virtual void run();
private:
};
GetAgent(SharedMemoryRpcAgent)
SharedMemoryRpcAgent::SharedMemoryRpcAgent()
{
}

SharedMemoryRpcAgent::~SharedMemoryRpcAgent()
{

}

void SharedMemoryRpcAgent::initialize()
{
	// 设置全局 logger
	spdlog::set_default_logger(m_loggerPtr);
	// 打印字符串
	spdlog::info("{} initialize", this->m_agentName);
}
void SharedMemoryRpcAgent::run()
{
	try
	{
		spdlog::info("{} run", this->m_agentName);
		Creator();
		Consumer(this);

	}
	catch (std::exception& e)
	{
		spdlog::error("{} run fail", this->m_agentName);
	}
}
