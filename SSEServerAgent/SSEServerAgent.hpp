

#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/version.hpp>
#include <boost/asio/dispatch.hpp>
#include <boost/asio/strand.hpp>
#include <boost/config.hpp>
#include <algorithm>
#include <cstdlib>
#include <functional>
#include <iostream>
#include <memory>
#include <string>
#include <thread>
#include <vector>
#include <map>
#include <ThreadSafeQueue.hpp>

#include <future>
#include <thread>
namespace beast = boost::beast;         // from <boost/beast.hpp>
namespace http = beast::http;           // from <boost/beast/http.hpp>
namespace net = boost::asio;            // from <boost/asio.hpp>
using tcp = boost::asio::ip::tcp;       // from <boost/asio/ip/tcp.hpp>

//sse Function map
std::map<std::string, std::function<void(ThreadSafeQueue<std::string>&)>> sseFuncMap; 

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



//------------------------------------------------------------------------------

// Report a failure
void fail(beast::error_code ec, char const* what)
{
    std::cerr << what << ": " << ec.message() << "\n";
}

// Handles an HTTP server connection
class session : public std::enable_shared_from_this<session>
{
    beast::tcp_stream stream_;
    beast::flat_buffer buffer_;
    std::shared_ptr<std::string const> doc_root_;
    http::request<http::string_body> req_;
    int sse=0;

public:
    // Take ownership of the stream
    session(
        tcp::socket&& socket,
        std::shared_ptr<std::string const> const& doc_root)
        : stream_(std::move(socket))
        , doc_root_(doc_root)
    {
    }


    template <class Body, class Allocator>
    void handle_request(
        beast::string_view doc_root,
        http::request<Body, http::basic_fields<Allocator>>&& req)
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
            http::message_generator(bad_request("Unknown HTTP-method"));
            return;
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
            http::message_generator(bad_request("Illegal request-target"));
            return ;

        }


        // Handle the case where the file doesn't exist
        if (sseFuncMap.find(tokens[2]) == sseFuncMap.end())
        {
            //send_response(not_found(req.target()));
            //return;
        }


        // Respond to HEAD request
        if (req.method() == http::verb::head)
        {
            http::response<http::empty_body> res{ http::status::ok, req.version() };
            res.set(http::field::server, BOOST_BEAST_VERSION_STRING);
            res.set(http::field::content_type, "text/event-stream");
            res.set(http::field::cache_control, "no-cache");
            res.keep_alive(req.keep_alive());
            send_response(std::move(res));
            return;
        }

        // Respond to GET request
        http::response<http::string_body> res;

        // 设置CORS头
        res.set(http::field::access_control_allow_origin, "*"); // 允许所有域访问

        // 根据请求方法设置响应内容
        if (req.method() == http::verb::options)
        {
            res.set(http::field::allow, "GET, HEAD, POST, OPTIONS");
            send_response(std::move(res));
        }
        else
        {

            ThreadSafeQueue<std::string> tsq;
            std::async(std::launch::async,std::bind(sseFuncMap[tokens[2]],tsq)).get();




            std::async(std::launch::async,
                [&]()
                {
                    while (0)
                    {
                        std::cout << "out";
                        res.version(req.version());
                        res.result(http::status::ok);
                        std::string outStr(std::move(tsq.wait_and_pop()));
                        res.body() = outStr.c_str();
                        //res.body() = "retry: 10000\nid: eef0128b-48b9-44f7-bbc6-9cc90d32ac4f\ndata: {\ndata: \"name\" : \"zhangsan\",\ndata : \"age\", 25\ndata :}\n\n";
                        res.set(http::field::server, BOOST_BEAST_VERSION_STRING);
                        res.set(http::field::content_type, "text/event-stream");
                        res.content_length(res.body().size());
                        if (outStr.find("\n\n") != std::string::npos)
                        {
                            res.keep_alive(false);
                            send_response(std::move(res));
                            return;
                        }
                        else
                        {
                            res.keep_alive(true);
                            send_response(std::move(res));
                        }


                    }
                    return;

                }
            ).get();
        


        }
        return;
    }

    // Start the asynchronous operation
    void run()
    {
        // We need to be executing within a strand to perform async operations
        // on the I/O objects in this session. Although not strictly necessary
        // for single-threaded contexts, this example code is written to be
        // thread-safe by default.
        net::dispatch(stream_.get_executor(),
            beast::bind_front_handler(
                &session::do_read,
                shared_from_this()));
    }

    void do_read()
    {
        // Make the request empty before reading,
        // otherwise the operation behavior is undefined.
        req_ = {};

        // Set the timeout.
        stream_.expires_after(std::chrono::seconds(30));

        // Read a request
        http::async_read(stream_, buffer_, req_,
            beast::bind_front_handler(
                &session::on_read,
                shared_from_this()));
    }

    void on_read(beast::error_code ec,std::size_t bytes_transferred)
    {

        boost::ignore_unused(bytes_transferred);

        // This means they closed the connection
        if (ec == http::error::end_of_stream)
            return do_close();

        if (ec)
            return fail(ec, "read");

        // Send the response
        std::string outStr;
        std::ostringstream ss;
        ss << beast::make_printable(buffer_.data());       
        outStr = ss.str();
        std::cout << "on_read" << std::endl<< outStr<<std::endl;

            handle_request(*doc_root_, std::move(req_));
    }

    void send_response(http::message_generator&& msg)
    {        
        bool keep_alive = msg.keep_alive();



        
        // Write the response
        beast::async_write(
            stream_,
            std::move(msg),
            beast::bind_front_handler(
                &session::on_write, shared_from_this(), keep_alive));
        


    }

    void on_write(
        bool keep_alive,
        beast::error_code ec,
        std::size_t bytes_transferred)
    {
        boost::ignore_unused(bytes_transferred);

        if (ec)
            return fail(ec, "write");

        if (!keep_alive)
        {
            // This means we should close the connection, usually because
            // the response indicated the "Connection: close" semantic.
            return do_close();
        }

        // Read another request
        do_read();
    }

    void do_close()
    {
        // Send a TCP shutdown
        beast::error_code ec;
        stream_.socket().shutdown(tcp::socket::shutdown_send, ec);

        // At this point the connection is closed gracefully
    }
};

//------------------------------------------------------------------------------

// Accepts incoming connections and launches the sessions
class listener : public std::enable_shared_from_this<listener>
{
    net::io_context& ioc_;
    tcp::acceptor acceptor_;
    std::shared_ptr<std::string const> doc_root_;

public:
    listener(
        net::io_context& ioc,
        tcp::endpoint endpoint,
        std::shared_ptr<std::string const> const& doc_root)
        : ioc_(ioc)
        , acceptor_(net::make_strand(ioc))
        , doc_root_(doc_root)
    {
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
            return; // To avoid infinite loop
        }
        else
        {
            // Create the session and run it
            std::make_shared<session>(
                std::move(socket),
                doc_root_)->run();
        }

        // Accept another connection
        do_accept();
    }
};

//------------------------------------------------------------------------------

int main(int argc, char* argv[])
{
    // Check command line arguments.

    auto const address = net::ip::make_address("127.0.0.1");
    auto const port = 8082;
    auto const doc_root = std::make_shared<std::string>("C:\\VulkanSDK\\1.3.296.0");
    auto const threads = 1;

    // The io_context is required for all I/O
    net::io_context ioc{ threads };

    // Create and launch a listening port
    std::make_shared<listener>(
        ioc,
        tcp::endpoint{ address, port },
        doc_root)->run();

    ioc.run();

    return EXIT_SUCCESS;
}
