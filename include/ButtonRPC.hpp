


#ifndef _BUTTONRPC_H_
#define _BUTTONRPC_H_

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
#include "Serializer.hpp"


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
	ButtonRPC() : context_(1) {}
	~ButtonRPC() { context_=NULL; }

	void as_client(std::string_view ip, int port)
	{
		role_ = RPC_CLIENT;
		socket_ = std::unique_ptr<nn_socket_pp, std::function<void(nn_socket_pp*)>>(new nn_socket_pp(), [](nn_socket_pp* sock)
			{
				sock->close();
				delete sock;
				sock = nullptr;
			});
		std::string addr = "tcp://" + std::string(ip) + ":" + std::to_string(port);
		socket_->connect(addr.c_str());
	}

	void as_server(std::string_view ip, int port)
	{
		role_ = RPC_SERVER;
		socket_ = std::unique_ptr<nn_socket_pp, std::function<void(nn_socket_pp*)>>(new nn_socket_pp(), [](nn_socket_pp* sock)
			{
				sock->close();
				delete sock;
				sock = nullptr;
			});
		std::string addr = "tcp://" + std::string(ip) + ":" + std::to_string(port);
		socket_->bind(addr.c_str());
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

				std::cout << "function not regist, name=" << funcname << std::endl;
				result << static_cast<int>(RPC_ERR_FUNCTION_NOT_REGUIST);
				result << std::string("function not bind: " + funcname);
			}
			auto func = mapFunctions_[funcname];
			func(&result, (char*)sr.data(), sr.size());
			//Serializer* rptr = call_(funcname, sr.data(), sr.size());
			this->send(result.data(), result.size());
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
		std::vector<char> request;
		request.resize(sr.size()+1);
		memcpy(request.data(), sr.data(), sr.size());
		this->send(request.data(), sr.size());
		void* reply=nullptr;
		int reply_size= this->recv(&reply);
		response_t<R> val;
		if (reply_size<=0)
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

private:
	std::map<std::string, std::function<void(Serializer*, const char*, int)>> mapFunctions_;
	int context_;
	std::unique_ptr<nn_socket_pp, std::function<void(nn_socket_pp*)>> socket_;
	int role_;
};

#endif
