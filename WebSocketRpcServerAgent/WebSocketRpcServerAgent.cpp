
#include <boost/beast/core.hpp>
#include <boost/beast/websocket.hpp>
#include <boost/asio/dispatch.hpp>
#include <boost/asio/strand.hpp>
#include <algorithm>
#include <cstdlib>
#include <functional>
#include <iostream>
#include <memory>
#include <string>
#include <thread>
#include <vector>
#include <AgentBase.hpp>
#include <future>
#include <thread>
#include <nlohmann/json.hpp>
namespace beast = boost::beast;         // from <boost/beast.hpp>
namespace http = beast::http;           // from <boost/beast/http.hpp>
namespace websocket = beast::websocket; // from <boost/beast/websocket.hpp>
namespace net = boost::asio;            // from <boost/asio.hpp>
using tcp = boost::asio::ip::tcp;       // from <boost/asio/ip/tcp.hpp>

//------------------------------------------------------------------------------

// Report a failure
void fail(beast::error_code ec, char const* what)
{
    std::cerr << what << ": " << ec.message() << "\n";
}

// Echoes back all received WebSocket messages
class session : public std::enable_shared_from_this<session>
{
    websocket::stream<beast::tcp_stream> ws_;
    beast::flat_buffer buffer_;
    AgentBase* agentPtr;

public:
    // Take ownership of the socket
    explicit
        session(tcp::socket&& socket, AgentBase* agentPtr)
        : ws_(std::move(socket))
    {
        this->agentPtr = agentPtr;
    }

    // Get on the correct executor
    void run()
    {
        // We need to be executing within a strand to perform async operations
        // on the I/O objects in this session. Although not strictly necessary
        // for single-threaded contexts, this example code is written to be
        // thread-safe by default.
        net::dispatch(ws_.get_executor(),
            beast::bind_front_handler(
                &session::on_run,
                shared_from_this()));
    }

    // Start the asynchronous operation
    void on_run()
    {
        // Set suggested timeout settings for the websocket
        ws_.set_option(
            websocket::stream_base::timeout::suggested(
                beast::role_type::server));

        // Set a decorator to change the Server of the handshake
        ws_.set_option(websocket::stream_base::decorator(
            [](websocket::response_type& res)
            {
                res.set(http::field::server,
                    std::string(BOOST_BEAST_VERSION_STRING) +
                    " websocket-server-async");
            }));
        // Accept the websocket handshake
        ws_.async_accept(
            beast::bind_front_handler(
                &session::on_accept,
                shared_from_this()));
    }

    void on_accept(beast::error_code ec)
    {
        if (ec)
            return fail(ec, "accept");
        // Read a message
        do_read();
    }

    void do_read()
    {
        // Read a message into our buffer
        ws_.async_read(
            buffer_,
            beast::bind_front_handler(
                &session::on_read,
                shared_from_this()));
    }

    void on_read(
        beast::error_code ec,
        std::size_t bytes_transferred)
    {
        boost::ignore_unused(bytes_transferred);
        // This indicates that the session was closed
        if (ec == websocket::error::closed)
            return;
        if (ec)
            return fail(ec, "read");



        std::string outStr;
        std::ostringstream ss;
        ss << beast::make_printable(buffer_.data());

        outStr = ss.str();
     
        nlohmann::json rpcDataJson = nlohmann::json::parse(outStr);
        std::string funcName(std::move(rpcDataJson["name"].get<std::string>()));
        std::string funcArg(std::move(rpcDataJson["arg"].get<std::string>()));
        std::string funcRes;
        std::function<int(const std::string&, std::string&)> rpcFunc;

        //默认成功调用
        int resCode = 1;

        if ((*(agentPtr->m_getRPCFuncPtr))(std::move(rpcDataJson["name"].get<std::string>()), rpcFunc) != 0)
        {
            resCode = 2;
        }
        else
        {
            //检测是否超时
            rpcFunc(funcArg, funcRes);

        }

    //state: '0',
    //    timeout : '1',
    //    name : 'publish_string',
    //    arg : '',
    //    res : '',
        rpcDataJson["state"] = resCode;
        rpcDataJson["res"] = funcRes;
        ws_.text(ws_.got_text());
        ws_.async_write(
            boost::asio::buffer(rpcDataJson.dump()),
            beast::bind_front_handler(
                &session::on_write,
                shared_from_this()));
    }

    void on_write(
        beast::error_code ec,
        std::size_t bytes_transferred)
    {
        boost::ignore_unused(bytes_transferred);

        if (ec)
            return fail(ec, "write");

        // Clear the buffer
        buffer_.consume(buffer_.size());

        // Do another read
        do_read();
    }
};

//------------------------------------------------------------------------------

// Accepts incoming connections and launches the sessions
class listener : public std::enable_shared_from_this<listener>
{
    net::io_context& ioc_;
    tcp::acceptor acceptor_;
    AgentBase* agentPtr;

public:
    listener(
        net::io_context& ioc,
        tcp::endpoint endpoint, AgentBase* agentPtr)
        : ioc_(ioc)
        , acceptor_(ioc)
    {
        this->agentPtr = agentPtr;
        beast::error_code ec;

        // Open the acceptor
        acceptor_.open(endpoint.protocol(), ec);
        if (ec)
        {
            fail(ec, "open");
            return;
        }

        // Allow address reuse
        acceptor_.set_option(net::socket_base::reuse_address(true), ec);
        if (ec)
        {
            fail(ec, "set_option");
            return;
        }

        // Bind to the server address
        acceptor_.bind(endpoint, ec);
        if (ec)
        {
            fail(ec, "bind");
            return;
        }

        // Start listening for connections
        acceptor_.listen(
            net::socket_base::max_listen_connections, ec);
        if (ec)
        {
            fail(ec, "listen");
            return;
        }
    }

    // Start accepting incoming connections
    void run()
    {
        do_accept();
    }

private:
    void do_accept()
    {
        // The new connection gets its own strand
        acceptor_.async_accept(
            net::make_strand(ioc_),
            beast::bind_front_handler(
                &listener::on_accept,
                shared_from_this()));
    }

    void on_accept(beast::error_code ec, tcp::socket socket)
    {
        if (ec)
        {
            fail(ec, "accept");
        }
        else
        {
            // Create the session and run it
            std::make_shared<session>(std::move(socket), agentPtr)->run();
        }

        // Accept another connection
        do_accept();
    }
};

class WebSocketRpcAgent : public AgentBase
{



public:

    WebSocketRpcAgent();
    virtual ~WebSocketRpcAgent();
    //初始化
    virtual void initialize();
    //运行
    virtual void run();
private:
};
GetAgent(WebSocketRpcAgent)
WebSocketRpcAgent::WebSocketRpcAgent()
{
}

WebSocketRpcAgent::~WebSocketRpcAgent()
{

}

void WebSocketRpcAgent::initialize()
{
    // 设置全局 logger
    spdlog::set_default_logger(m_loggerPtr);
    // 打印字符串
    spdlog::info("{} initialize", this->m_agentName);

}
void WebSocketRpcAgent::run()
{
    try
    {        
        
       
        
        auto const address = net::ip::make_address("127.0.0.1");
        auto const port = 8080;

        net::io_context ioc;

        std::shared_ptr<listener> listenerPtr = std::make_shared<listener>(ioc, tcp::endpoint{ address, port }, this);


        spdlog::info("{} run", this->m_agentName);
        listenerPtr->run();  
        ioc.run();

    }
    catch (std::exception& e)
    {
        spdlog::error("{} run fail", this->m_agentName);
    }
}
