
#include <iostream>
#include <boost/asio.hpp>
#include <boost/bind.hpp>
#include <boost/enable_shared_from_this.hpp>
#include <boost/lockfree/queue.hpp>
#include <thread>
#include <vector>

#include <queue>
#include <boost/thread/mutex.hpp>
#include <boost/thread/condition_variable.hpp>

using boost::asio::ip::tcp;

const int PORT = 8001;
const int THREAD_NUM = 4;


template<typename Data>
class concurrent_queue
{
private:
    std::queue<Data> the_queue;
    mutable boost::mutex the_mutex;
    boost::condition_variable the_condition_variable;
public:
    void push(Data const& data)
    {
        boost::mutex::scoped_lock lock(the_mutex);
        the_queue.push(data);
        lock.unlock();
        the_condition_variable.notify_one();
    }

    bool empty() const
    {
        boost::mutex::scoped_lock lock(the_mutex);
        return the_queue.empty();
    }

    bool try_pop(Data& popped_value)
    {
        boost::mutex::scoped_lock lock(the_mutex);
        if(the_queue.empty())
        {
            return false;
        }

        popped_value=the_queue.front();
        the_queue.pop();
        return true;
    }

    void wait_and_pop(Data& popped_value)
    {
        boost::mutex::scoped_lock lock(the_mutex);
        while(the_queue.empty())
        {
            the_condition_variable.wait(lock);
        }

        popped_value=the_queue.front();
        the_queue.pop();
    }

};

class connection : public boost::enable_shared_from_this<connection>
{
private:
    tcp::socket sock;
    char data[1024];

public:
    typedef boost::shared_ptr<connection> pointer;
    connection(boost::asio::io_service& io_service): sock(io_service){}

    static pointer create(boost::asio::io_service& io_service)
    {
        return pointer(new connection(io_service));
    }

    tcp::socket& socket()
    {
        return sock;
    }

    void start()
    {
        sock.async_read_some(
                boost::asio::buffer(data, 1024),
                boost::bind(&connection::handle_read,
                            shared_from_this(),
                            boost::asio::placeholders::error,
                            boost::asio::placeholders::bytes_transferred)
                            );

        sock.async_write_some(
                boost::asio::buffer(data, 1024),
                boost::bind(&connection::handle_write,
                            shared_from_this(),
                            boost::asio::placeholders::error,
                            boost::asio::placeholders::bytes_transferred)
                            );
    }

    void handle_read(const boost::system::error_code& err, size_t bytes_transferred)
    {
        if (err) {
            std::cerr << err.message() << std::endl;
            sock.close();
        }
    }
    void handle_write(const boost::system::error_code& err, size_t bytes_transferred)
    {
        if (err) {
            std::cerr << err.message() << std::endl;
            sock.close();
        }
    }
};

// SERVER
class Server
{
private:
    // Acceptor for new socket connections to the server
    tcp::acceptor acceptor_;
    concurrent_queue<connection::pointer*>* conn_queue;
    void start_accept()
    {
        connection::pointer connection = connection::create(acceptor_.get_io_service());
        acceptor_.async_accept(connection->socket(),
                               boost::bind(&Server::handle_accept, this, connection,
                                           boost::asio::placeholders::error));
    }
public:
    Server(boost::asio::io_service& io_service, concurrent_queue<connection::pointer*> *q): acceptor_(io_service, tcp::endpoint(tcp::v4(), PORT)), conn_queue(q)
    {
        start_accept();
    }
    void handle_accept(connection::pointer connection, const boost::system::error_code& err)
    {
        if (!err) {
            conn_queue->push(&connection);
        }
        start_accept();
    }
};

// MAIN

void processor(concurrent_queue<connection::pointer*> & q) {
    for (;;) {
        connection::pointer* conn;
        q.wait_and_pop(conn);
        (*conn)->start();
    }
}


int main(int argc, char *argv[])
{

    boost::asio::io_service io_service;
    concurrent_queue<connection::pointer*> q;
    std::vector<std::thread> all_threads;

    for (int i = 0; i < THREAD_NUM; ++i) {
        all_threads.push_back(std::thread(processor, std::ref(q)));
        all_threads.back().detach();
    }


    Server server(io_service, &q);
    io_service.run();

    return 0;
}