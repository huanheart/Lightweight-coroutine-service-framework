#include "tcp_server.h"
#include<muduo/base/Logging.h>

#include"../util/config.h"

using namespace std;

namespace sylar {

static uint64_t g_tcp_server_read_timeout =(uint64_t)(60 * 1000 * 2) ;



//设置为true表明服务还在停机中，并没有开启
TcpServer::TcpServer(sylar::IOManager* worker,sylar::IOManager* io_worker,sylar::IOManager* accept_worker):m_worker(worker),m_ioWorker(io_worker),m_acceptWorker(accept_worker),m_recvTimeout(g_tcp_server_read_timeout),m_name("sylar/1.0.0"),m_isStop(true)   {

}

TcpServer::~TcpServer() {
    for(auto& i : m_socks) {
        i->close();
    }
    m_socks.clear();
}



bool TcpServer::bind(sylar::Address::ptr addr, bool ssl) {
    std::vector<Address::ptr> addrs;
    std::vector<Address::ptr> fails;
    addrs.push_back(addr);
    return bind(addrs, fails, ssl);
}

bool TcpServer::bind(const std::vector<Address::ptr>& addrs
                        ,std::vector<Address::ptr>& fails
                        ,bool ssl) {
    m_ssl = ssl;
    for(auto& addr : addrs) {
        Socket::ptr sock = ssl ? SSLSocket::CreateTCP(addr) : Socket::CreateTCP(addr);
        if(!sock->bind(addr) ) {
            if(Config::get_instance()->get_close_log()==0) {
                LOG_INFO << "bind fail errno=" << errno << " errstr=" << strerror(errno) << " addr=["
                         << addr->toString() << "]";
            }
            fails.push_back(addr);
            continue;
        }
        if(!sock->listen()) {
            if(Config::get_instance()->get_close_log()==0) {
                LOG_INFO << "listen fail errno="
                         << errno << " errstr=" << strerror(errno)
                         << " addr=[" << addr->toString() << "]";
            }
            fails.push_back(addr);
            continue;
        }
        m_socks.push_back(sock);
    }

    if(!fails.empty()) {
        m_socks.clear();
        return false;
    }

    return true;
}

void TcpServer::startAccept(Socket::ptr sock) {
    while(!m_isStop) {
        //一个sockfd描述符监听多个client，调用属于每个client独特的handleClient做处理
        Socket::ptr client = sock->accept();
        if(client) {
            client->setRecvTimeout(m_recvTimeout);
            m_ioWorker->scheduleLock(std::bind(&TcpServer::handleClient,
                        shared_from_this(), client));
        } else {
            if(Config::get_instance()->get_close_log()==0) {
                LOG_INFO << "accept errno=" << errno
                         << " errstr=" << strerror(errno);
            }
        }

    }

}

bool TcpServer::start() {
    if(!m_isStop) {
        return true;
    }
    m_isStop = false;
    for(auto& sock : m_socks) {
        m_acceptWorker->scheduleLock(std::bind(&TcpServer::startAccept,
                    shared_from_this(), sock));
    }
    return true;
}

void TcpServer::stop() {
    m_isStop = true;
    auto self = shared_from_this();
    m_acceptWorker->scheduleLock([this, self]() {
        for(auto& sock : m_socks) {
            sock->cancelAll();
            sock->close();
        }
        m_socks.clear();
    });
}



bool TcpServer::loadCertificates(const std::string& cert_file, const std::string& key_file) {
    for(auto& i : m_socks) {
        auto ssl_socket = std::dynamic_pointer_cast<SSLSocket>(i);
        if(ssl_socket) {
            if(!ssl_socket->loadCertificates(cert_file, key_file)) {
                return false;
            }
        }
    }
    return true;
}

std::string TcpServer::toString(const std::string& prefix) {
    std::stringstream ss;
    ss << prefix << "[type=" << m_type
       << " name=" << m_name << " ssl=" << m_ssl
       << " worker=" << (m_worker ? m_worker->getName() : "")
       << " accept=" << (m_acceptWorker ? m_acceptWorker->getName() : "")
       << " recv_timeout=" << m_recvTimeout << "]" << std::endl;
    std::string pfx = prefix.empty() ? "    " : prefix;
    for(auto& i : m_socks) {
        ss << pfx << pfx << *i << std::endl;
    }
    return ss.str();
}
void TcpServer::handleClient(Socket::ptr client) {
        std::cout<<" TcpServer handleClient need override !!!"<<std::endl;
    }

}

