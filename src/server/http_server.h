#ifndef HTTPSERVER_H
#define HTTPSERVER_H
#include<string>
#include "tcp_server.h"

#include"../cglmysql/sql_connection_pool.h"
#include"../util/socket.h"
using namespace std;
const int MAX_FD=65536;


class http_server :public sylar::TcpServer
{
public:
    //这两个是间接给到http的
    void initmysql_result(Connection_pool *conn_pool);
    void initproxy(int decideproxy);
    ////默认长连接
    http_server(bool keepalive=true
            ,sylar::IOManager* worker = sylar::IOManager::GetThis()
            ,sylar::IOManager* io_worker = sylar::IOManager::GetThis()
            ,sylar::IOManager* accept_worker = sylar::IOManager::GetThis());
    ~http_server();
public:
    char * m_root=nullptr;
    string m_user;
    string m_password;
    string m_database_name;

    void handleClient(std::shared_ptr<sylar::Socket> client) override;
private:
    bool m_isKeepalive;
};

#endif