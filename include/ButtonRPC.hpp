

#ifndef _BUTTONRPC_H_
#define _BUTTONRPC_H_

#include <boost/interprocess/managed_shared_memory.hpp>
#include <boost/interprocess/allocators/allocator.hpp>
#include <boost/interprocess/containers/string.hpp>
#include <boost/lockfree/queue.hpp>
#include <iostream>
#include <map>
#include <functional>
#include <tuple>
#include <memory>
#include "Serializer.hpp"
#if defined(_WIN32)
#pragma comment(lib,"ws2_32.lib")
#pragma comment(lib,"Mswsock.lib")
#endif



#define NN_STATIC_LIB 1
#include <nanomsg/nn.h>
#include <nanomsg/bus.h> 
#include <nanomsg/pair.h>

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

typedef std::pair<IPCHandleMwMrQueueT*, boost::interprocess::managed_shared_memory::size_type> ResultT;
void Creator(std::string_view port)
{

	// 删除遗留的共享内存
	boost::interprocess::shared_memory_object::remove(port.data());

	// 构建新的托管共享内存区
	boost::interprocess::managed_shared_memory segment(boost::interprocess::create_only,
		port.data(),  //segment name
		65536 * 1024 * 10);

	// 定义托管共享内存区的分配器
	const MyIPCHandleAllocatorT alloc_inst(segment.get_segment_manager());

	try
	{

		// 创建共享内存中的用于传递IPCHandleT的无锁队列,放请求
		IPCHandleMwMrQueueT* pMyIPCHandleQueueRequest = segment.construct<IPCHandleMwMrQueueT>("IPCHandleQueueRequest")(alloc_inst);
		// 创建共享内存中的用于传递IPCHandleT的无锁队列,放结果
		IPCHandleMwMrQueueT* pMyIPCHandleQueueResult = segment.construct<IPCHandleMwMrQueueT>("IPCHandleQueueResult")(alloc_inst);

	}
	catch (std::exception& e)
	{
		std::cout << e.what() << std::endl;
	}



}



class nn_socket_pp
{
	int socket;
public:
	nn_socket_pp()
	{
		this->socket = nn_socket(1, NN_PAIR);
	}

	void close()
	{
		nn_close(this->socket);
	}
	int bind(const char* url)
	{
		return nn_bind(socket, url);

	}
	int send(const char* data, int size)
	{
		return nn_send(socket, data, size, 0);
	}

	int recv(void** data)
	{
		return nn_recv(socket, data, NN_MSG, 0);
	}
	int connect(const char* url)
	{
		while (nn_connect(socket, url)<0)
		{

		}
		return 0;
	}
	int set_timeout(int timeout)
	{
		return nn_setsockopt(socket, NN_SOL_SOCKET, NN_LINGER, &timeout, sizeof(timeout));
	}

};
template<typename T>
struct type_xx { typedef T type; };

template<>
struct type_xx<void> { typedef int8_t type; };

// 打包帮助模板
template<typename Tuple, std::size_t... Index>
void package_Args_impl(Serializer& sr, const Tuple& t, std::index_sequence<Index...>)
{
	std::initializer_list<int> { (sr << std::get<Index>(t), 0)... };
}

template<typename... Args>
void package_Args(Serializer& sr, const std::tuple<Args...>& t)
{
	package_Args_impl(sr, t, std::index_sequence_for<Args...>{});
}

// 用tuple做参数调用函数模板类
template<typename Function, typename Tuple, std::size_t... Index>
decltype(auto) invoke_impl(Function&& func, Tuple&& t, std::index_sequence<Index...>)
{
	return func(std::get<Index>(std::forward<Tuple>(t))...);
}

template<typename Function, typename Tuple>
decltype(auto) invoke(Function&& func, Tuple&& t)
{
	constexpr auto size = std::tuple_size<typename std::decay<Tuple>::type>::value;
	return invoke_impl(std::forward<Function>(func), std::forward<Tuple>(t), std::make_index_sequence<size>{});
}

template<typename R, typename F, typename ArgsTuple>
typename std::enable_if<!std::is_same<R, void>::value, typename type_xx<R>::type >::type
call_helper(F f, ArgsTuple args)
{
	return invoke(f, args);
}

enum rpc_role {
	RPC_CLIENT,
	RPC_SERVER
};
enum rpc_errcode {
	RPC_ERR_SUCCESS = 0,
	RPC_ERR_FUNCTION_NOT_REGUIST,
	RPC_ERR_RECV_TIMEOUT
};

template <typename T>
class response_t
{
public:
	response_t()
		: code_(RPC_ERR_SUCCESS)
	{
		message_.clear();
	}
	int code() const { return code_; }
	std::string message() const { return message_; }
	T value() const { return value_; }

	void set_value(const T& val) { value_ = val; }
	void set_code(int code) { code_ = (int)code; }
	void set_message(const std::string& msg) { message_ = msg; }

	friend Serializer& operator>>(Serializer& in, response_t<T>& d)
	{
		in >> d.code_ >> d.message_;
		if (d.code_ == 0)
			in >> d.value_;
		return in;
	}
	friend Serializer& operator<<(Serializer& out, response_t<T>& d)
	{
		out << d.code_ << d.message_ << d.value_;
		return out;
	}

private:
	int code_;
	std::string message_;
	T value_;
};

class ButtonRPC
{
public:
	ButtonRPC() : context_(1) 
	{
		
		
	}
	~ButtonRPC() { context_=NULL; }

	void as_client(std::string_view ip, int port)
	{
		role_ = RPC_CLIENT;
		if (ip=="shm")
		{			
			mode_=1;
			port_ = port;
			//Producer(std::to_string(port),);


		}
		else
		{
			mode_ = 2;
			socket_ = std::unique_ptr<nn_socket_pp, std::function<void(nn_socket_pp*)>>(new nn_socket_pp(), [](nn_socket_pp* sock)
				{
					sock->close();
					delete sock;
					sock = nullptr;
				});
			std::string addr = "tcp://" + std::string(ip) + ":" + std::to_string(port);
			socket_->connect(addr.c_str());
		}

	}

	void as_server(std::string_view ip, int port)
	{
		role_ = RPC_SERVER;
		if (ip == "shm")
		{
			mode_ = 1;
			port_ = port;
			Creator(std::to_string(port_));


		}
		else
		{
			mode_ = 2;
			socket_ = std::unique_ptr<nn_socket_pp, std::function<void(nn_socket_pp*)>>(new nn_socket_pp(), [](nn_socket_pp* sock)
				{
					sock->close();
					delete sock;
					sock = nullptr;
				});
			std::string addr = "tcp://" + std::string(ip) + ":" + std::to_string(port);
			socket_->bind(addr.c_str());
		}

	}

	bool send(const char* data,int size)
	{

		int res = socket_->send(data,size);
		return res;
	}
	int recv(void** data)
	{
		return socket_->recv(data);
	}
	void set_timeout(uint32_t ms)
	{
		if (role_ == RPC_CLIENT)
		{
			socket_->set_timeout(ms);
		}

	}

	void run()
	{
		if (mode_==2)
		{
			while (1)
			{
				void *data=nullptr;
				int data_size=this->recv(&data);
				std::string a((char*)data, data_size);
				nn_freemsg(data);

				Buffer::ptr buffer = std::make_shared<Buffer>((char*)data, data_size);
				Serializer sr(buffer);
				std::string funcname;

				sr >> funcname;
				Serializer result;			
				if (mapFunctions_.find(funcname) == mapFunctions_.end())
				{

					result << static_cast<int>(RPC_ERR_FUNCTION_NOT_REGUIST);
					result << std::string("function not bind: " + funcname);
				}
				auto func = mapFunctions_[funcname];
				func(&result, (char*)sr.data(), sr.size());
				this->send(result.data(), result.size());
			}
		}
		else if (mode_==1)
		{
			//附接目标共享内存
			boost::interprocess::managed_shared_memory segment(boost::interprocess::open_only,std::to_string(port_).data());  //segment name
			ResultT res1 = segment.find<IPCHandleMwMrQueueT>("IPCHandleQueueRequest");
			ResultT res2 = segment.find<IPCHandleMwMrQueueT>("IPCHandleQueueResult");
			if (!res1.second || !res2.second)
			{
				return;
			}
			else
			{
			}

			IPCHandleMwMrQueueT* pMyIPCHandleQueue1 = res1.first;
			IPCHandleMwMrQueueT* pMyIPCHandleQueue2 = res2.first;
			while (1)
			{
				IPCHandleT requestHandle;
				if (pMyIPCHandleQueue1->pop(requestHandle))
				{

					//接收到消息后，通过handle找到对应的共享内存对象
					IPCHandleData* pIPCHandleData =(IPCHandleData*)segment.get_address_from_handle(requestHandle);
					if (pIPCHandleData->m_rpc.size()!=0)
					{
						Serializer sr(pIPCHandleData->m_rpc.data(), pIPCHandleData->m_rpc.size());
						std::string funcname;
						sr >> funcname;
						Serializer result;
						if (mapFunctions_.find(funcname) == mapFunctions_.end())
						{
							result << static_cast<int>(RPC_ERR_FUNCTION_NOT_REGUIST);
							result << std::string("function not bind: " + funcname);
						}
						pIPCHandleData->m_state = 1;
						auto func = mapFunctions_[funcname];
						func(&result, (char*)sr.data(), sr.size());						
						pIPCHandleData->m_rpc.resize(result.size());
						memcpy(pIPCHandleData->m_rpc.data(), result.data(), result.size());
						pMyIPCHandleQueue2->push(requestHandle);
					}
					else
					{
						pIPCHandleData->m_state = 0;
					}

				}

			}
		}

	}
	template <typename R, typename... Args>
	response_t<R> call(const std::string& name, Args... args)
	{
		using args_type = std::tuple<typename std::decay<Args>::type...>;
		args_type as = std::make_tuple(args...);
		Serializer sr;
		sr << name;
		package_Args(sr, as);
		return net_call<R>(sr);
	}

	template <typename F>
	void regist(const std::string& name, F func)
	{
		mapFunctions_[name] = std::bind(&ButtonRPC::callproxy<F>, this, func, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3);
	}

	template<typename F, typename S>
	void regist(const std::string& name, F func, S* s)
	{
		mapFunctions_[name] = std::bind(&ButtonRPC::callproxy<F, S>, this, func, s, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3);
	}

	template<typename F>
	void callproxy(F func, Serializer* pr, const char* data, int len)
	{
		callproxy_(func, pr, data, len);
	}
	template<typename F, typename S>
	void callproxy(F func, S* s, Serializer* pr, const char* data, int len)
	{
		callproxy_(func, s, pr, data, len);
	}

	// 函数指针
	template<typename R, typename... Args>
	void callproxy_(R(*func)(Args...), Serializer* pr, const char* data, int len) {
		callproxy_(std::function<R(Args...)>(func), pr, data, len);
	}

	template<typename R, typename C, typename S, typename... Args>
	void callproxy_(R(C::* func)(Args...), S* s, Serializer* pr, const char* data, int len) {

		using args_type = std::tuple<typename std::decay<Args>::type...>;

		Serializer sr(std::make_shared<Buffer>(data, len));
		args_type as = sr.get_tuple<args_type>(std::index_sequence_for<Args...>{});

		auto ff = [=](Args... args)->R {
			return (s->*func)(args...);
			};
		typename type_xx<R>::type r = call_helper<R>(ff, as);

		response_t<R> response;
		response.set_code(RPC_ERR_SUCCESS);
		response.set_message("success");
		response.set_value(r);
		(*pr) << response;
	}

	template<typename R, typename... Args>
	void callproxy_(std::function<R(Args... args)> func, Serializer* pr, const char* data, int len)
	{
		using args_type = std::tuple<typename std::decay<Args>::type...>;

		Serializer sr(std::make_shared<Buffer>(data, len));
		args_type as = sr.get_tuple<args_type>(std::index_sequence_for<Args...>{});

		typename type_xx<R>::type r = call_helper<R>(func, as);

		response_t<R> response;
		response.set_code(RPC_ERR_SUCCESS);
		response.set_message("success");
		response.set_value(r);
		(*pr) << response;
	}

	template<typename R>
	response_t<R> net_call(Serializer& sr)
	{
		if (mode_ == 2)
		{	
			std::vector<char> request;
			request.resize(sr.size() + 1);
			memcpy(request.data(), sr.data(), sr.size());

			this->send(request.data(), sr.size());
			void* reply = nullptr;
			int reply_size = this->recv(&reply);
			response_t<R> val;
			if (reply_size <= 0)
			{
				val.set_code(RPC_ERR_RECV_TIMEOUT);
				val.set_message("recv timeout");
				return val;
			}
			Serializer res((const char*)reply, reply_size);
			nn_freemsg(reply);
			res >> val;
			return val;
		}
		else if (mode_ == 1)
		{
			//附接目标共享内存
			boost::interprocess::managed_shared_memory segment(boost::interprocess::open_only, std::to_string(port_).data());  //segment name

			ResultT res1 = segment.find<IPCHandleMwMrQueueT>("IPCHandleQueueRequest");
			ResultT res2 = segment.find<IPCHandleMwMrQueueT>("IPCHandleQueueResult");

			response_t<R> val;
			if (!res1.second || !res2.second)
			{
				val.set_code(RPC_ERR_RECV_TIMEOUT);
				val.set_message("recv timeout");
				return val;
			}

			IPCHandleMwMrQueueT* pMyIPCHandleQueue1 = res1.first;
			IPCHandleMwMrQueueT* pMyIPCHandleQueue2 = res2.first;

			int id = 1;

			IPCHandleData* pIPCHandleData =
				segment.construct<IPCHandleData>
				(boost::interprocess::anonymous_instance)
				(shared_string::allocator_type(segment.get_segment_manager()));
			/*
			uint32_t m_clientPid;//客户端pid
			int m_state;//状态0:no call 1:call already*/
			pIPCHandleData->m_clientPid = getCurrentProcessId();
			pIPCHandleData->m_rpc.resize(sr.size());
			memcpy(pIPCHandleData->m_rpc.data(), sr.data(), sr.size());
			pIPCHandleData->m_state = 0;

			//获取共享内存对应的handle
			IPCHandleT curHandle = segment.get_handle_from_address(pIPCHandleData);

			//将该handle放入queue中，传递给对端
			pMyIPCHandleQueue1->push(curHandle);
			//拿出结果
			while (1)
			{
				IPCHandleT requestHandle;
				if (pMyIPCHandleQueue2->pop(requestHandle))
				{
					IPCHandleData* pIPCHandleData =(IPCHandleData*)segment.get_address_from_handle(requestHandle);
					if (pIPCHandleData->m_state==1)
					{
						Serializer res(pIPCHandleData->m_rpc.data(), pIPCHandleData->m_rpc.size());
						res >> val;
						return val;
					}
					else 
					{
						Serializer res("0", 1);
						res >> val;
						return val;
					}


				}
			}
		}

	}

private:
	std::map<std::string, std::function<void(Serializer*, const char*, int)>> mapFunctions_;
	int context_;
	std::unique_ptr<nn_socket_pp, std::function<void(nn_socket_pp*)>> socket_;
	int role_;
	//1:shm 2:nanomsg-tcp
	int mode_;

	int port_;
};

#endif
