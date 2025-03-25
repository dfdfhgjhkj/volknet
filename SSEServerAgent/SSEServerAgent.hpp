

#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/version.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/config.hpp>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <string>
#include <thread>
#include <map>
#include <ThreadSafeQueue.hpp>

#include <future>
#include <thread>
namespace beast = boost::beast;         // from <boost/beast.hpp>
namespace http = beast::http;           // from <boost/beast/http.hpp>
namespace net = boost::asio;            // from <boost/asio.hpp>
using tcp = boost::asio::ip::tcp;       // from <boost/asio/ip/tcp.hpp>

//sse Function map
std::map<std::string, std::function<void(ThreadSafeQueue<std::string>*)>> sseFuncMap;
void split(std::string& s, char delimiter, std::vector<std::string>& tokens)
{
    tokens.clear();
    std::string token;
    std::istringstream tokenStream(s);
    while (std::getline(tokenStream, token, delimiter)) {
        tokens.push_back(token);
    }
    return;
}


template <class Body, class Allocator>
bool
handle_request(
    tcp::socket& socket,
    beast::string_view doc_root,
    http::request<Body, http::basic_fields<Allocator>>&& req,
    beast::error_code& ec)
{
    // Returns a bad request response
    auto const bad_request =
        [&req](beast::string_view why)
        {
            http::response<http::string_body> res{ http::status::bad_request, req.version() };
            res.set(http::field::server, BOOST_BEAST_VERSION_STRING);
            res.set(http::field::content_type, "text/html");
            res.keep_alive(req.keep_alive());
            res.body() = std::string(why);
            res.prepare_payload();
            return res;
        };

    // Returns a not found response
    auto const not_found =
        [&req](beast::string_view target)
        {
            http::response<http::string_body> res{ http::status::not_found, req.version() };
            res.set(http::field::server, BOOST_BEAST_VERSION_STRING);
            res.set(http::field::content_type, "text/html");
            res.keep_alive(req.keep_alive());
            res.body() = "The resource '" + std::string(target) + "' was not found.";
            res.prepare_payload();
            return res;
        };

    // Returns a server error response
    auto const server_error =
        [&req](beast::string_view what)
        {
            http::response<http::string_body> res{ http::status::internal_server_error, req.version() };
            res.set(http::field::server, BOOST_BEAST_VERSION_STRING);
            res.set(http::field::content_type, "text/html");
            res.keep_alive(req.keep_alive());
            res.body() = "An error occurred: '" + std::string(what) + "'";
            res.prepare_payload();
            return res;
        };

    // Make sure we can handle the method
    if (req.method() != http::verb::get &&
        req.method() != http::verb::head)
    {
        beast::write(socket, http::message_generator(std::move(bad_request("Unknown HTTP-method"))));
        return false;
    }

    std::string target(req.target());
    std::vector<std::string> tokens;
    split(target, '/', tokens);

    // Request path must be absolute and not contain "..".
    if (
        req.target().empty() ||
        req.target()[0] != '/' ||
        req.target().find("..") != beast::string_view::npos ||
        tokens.size() != 3 ||
        tokens[1] != "sse"
        )
    {
        beast::write(socket, http::message_generator(std::move(bad_request("Illegal request-target"))), ec);
        return false;

    }


    if (sseFuncMap.find(tokens[2]) == sseFuncMap.end())
    {
        beast::write(socket, http::message_generator(std::move(not_found(req.target()))), ec);
        return false;
    }


    // Respond to HEAD request
    if (req.method() == http::verb::head)
    {
        http::response<http::empty_body> res{ http::status::ok, req.version() };
        res.set(http::field::server, BOOST_BEAST_VERSION_STRING);
        res.set(http::field::content_type, "text/event-stream");
        res.set(http::field::cache_control, "no-cache");
        res.keep_alive(req.keep_alive());
        beast::write(socket, http::message_generator(std::move(res)), ec);
        return false;
    }



    // 根据请求方法设置响应内容
    if (req.method() == http::verb::options)
    {
        http::response<http::string_body> res;
        res.set(http::field::access_control_allow_origin, "*");
        res.set(http::field::allow, "GET, HEAD, POST, OPTIONS");
        beast::write(socket, http::message_generator(std::move(res)), ec);
    }
    else
    {
        std::vector<std::future<void>> runFuncVector;

        ThreadSafeQueue<std::string> tsq;
        runFuncVector.push_back(std::async(std::launch::async, std::bind(sseFuncMap[tokens[2]], &tsq)));




        runFuncVector.push_back(std::async(std::launch::async,
            [&]()
            {
                int count = 1;
                while (1)
                {   http::response<http::string_body> res;
                    if (count)
                    {                    
                        res.set(http::field::access_control_allow_origin, "*");
                        std::cout << "out";
                        res.version(req.version());
                        res.result(http::status::ok);                    
                        res.set(http::field::server, BOOST_BEAST_VERSION_STRING);
                        res.set(http::field::content_type, "text/event-stream");
                        res.body()="retry: 10000\nevent: start\n";
                        beast::write(socket, http::message_generator(std::move(res)), ec);
                        count--;
                    }
                    std::string outStr(std::move(tsq.wait_and_pop()));


                    res.body() = outStr;
                    std::cout << outStr;

                    res.content_length(res.body().size());
                    if (outStr.find("\n\n") != std::string::npos)
                    {
                        std::cout << "end" << std::endl;
                        res.keep_alive(true);

                        beast::write(socket, http::message_generator(std::move(res)), ec);
                        return;
                    }
                    else
                    {
                        res.keep_alive(true);


                        // Send the response
                        beast::write(socket, http::message_generator(std::move(res)), ec);
                    }


                }
                return;

            }
        ));
        for (std::vector<std::future<void>>::iterator it = runFuncVector.begin(); it != runFuncVector.end(); it++)
        {
            it->get();
        }


    }
    return true;
}

//------------------------------------------------------------------------------

// Report a failure
void
fail(beast::error_code ec, char const* what)
{
    std::cerr << what << ": " << ec.message() << "\n";
}

// Handles an HTTP server connection
void
do_session(
    tcp::socket& socket,
    std::shared_ptr<std::string const> const& doc_root)
{
    beast::error_code ec;

    // This buffer is required to persist across reads
    beast::flat_buffer buffer;

    for (;;)
    {
        // Read a request
        http::request<http::string_body> req;
        http::read(socket, buffer, req, ec);
        if (ec == http::error::end_of_stream)
            break;
        if (ec)
            return fail(ec, "read");

        // Handle request
         bool keep_alive=handle_request(socket ,*doc_root, std::move(req), ec);



        if (ec)
            return fail(ec, "write");
        if (!keep_alive)
        {
            // This means we should close the connection, usually because
            // the response indicated the "Connection: close" semantic.
            break;
        }
    }

    // Send a TCP shutdown
    socket.shutdown(tcp::socket::shutdown_send, ec);

    // At this point the connection is closed gracefully
}

//------------------------------------------------------------------------------

int main()
{
    try
    {
        sseFuncMap["1"] = [](ThreadSafeQueue<std::string>* tsq)
            {
                int i = 10;
                while (i--)
                {
                    tsq->push(std::to_string(i) +   "次,喵\n");
                    Sleep(1000);
                    std::cout << "________" << i << std::endl;
                }
                tsq->push("喵喵喵\n\n");

            };
        auto const address = net::ip::make_address("127.0.0.1");
        auto const port = 8082;
        auto const doc_root = std::make_shared<std::string>("C:\\VulkanSDK\\1.3.296.0");
        auto const threads = 1;

        // The io_context is required for all I/O
        net::io_context ioc{ 1 };

        // The acceptor receives incoming connections
        tcp::acceptor acceptor{ ioc, {address, port} };
        for (;;)
        {
            // This will receive the new connection
            tcp::socket socket{ ioc };

            // Block until we get a connection
            acceptor.accept(socket);

            // Launch the session, transferring ownership of the socket
            std::thread{ std::bind(
                &do_session,
                std::move(socket),
                doc_root) }.detach();
        }
    }
    catch (const std::exception& e)
    {
        std::cerr << "Error: " << e.what() << std::endl;
        return EXIT_FAILURE;
    }
}
