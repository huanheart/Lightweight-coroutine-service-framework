
#include"http_server.h"
#include"http_conn.h"
#include"../CoroutineLibrary/hook.h"
#include<muduo/base/Logging.h>
#include"../util/config.h"

http_conn * users;   //用户http数组

http_server::http_server(bool keepalive
                        ,sylar::IOManager* worker
                        ,sylar::IOManager* io_worker
                        ,sylar::IOManager* accept_worker)
                        :TcpServer(worker,io_worker,accept_worker){
    users=new http_conn[MAX_FD];
    char server_path[200];
    m_isKeepalive=keepalive;
    getcwd(server_path, 200); //获取当前文件路径
    char root[13] = "/../src/root"; ////m_root和root不一样的(采用硬编码的形式）
    m_root = (char *)malloc(strlen(server_path) + strlen(root) + 1);
    strcpy(m_root, server_path);
    strcat(m_root, root); //追加
//    std::cout<<m_root<<std::endl;
    //共享同一个文件描述符（为了防止多定理这个epoll实例，我们只会在协程库中做操作）
}

void http_server::initmysql_result(Connection_pool *conn_pool) {
    users->initmysql_result(conn_pool);
}

void http_server::initproxy(int decideproxy) {
    users->initproxy(decideproxy);
}


http_server::~http_server(){
    if(m_root)
        delete m_root;
}

void http_server::handleClient(sylar::Socket::ptr client) {
    if(Config::get_instance()->get_close_log()==0) {
        LOG_INFO <<" http_server handleClient   !!!  "<<"and dealing "<<client->getSocket()<<" now ";
    }
    sylar::set_hook_enable(true);
    int client_socket=client->getSocket();
    if(!client->isValid()){
        client->close();
        return ;
    }

    users[client_socket].init(client,m_root,0,m_user,m_password,m_database_name,m_isKeepalive);
    while(client->isConnected()){
        if(users[client_socket].read_once()==false) {
            LOG_INFO <<" read_once return false";
            goto end;
        }
        if(Config::get_instance()->get_close_log()==0) {
            LOG_INFO <<users[client_socket].m_read_buf;
        }
        {
            connectionRAII mysqlcon(&users[client_socket].mysql, Connection_pool::get_instance());
        }
        if(users[client_socket].process()==false) {
            LOG_INFO <<" process return false";
            goto end;
        }
        if(users[client_socket].write()==false) {
            LOG_INFO <<" write return false";
            goto end;
        }
    }
end :
    if(Config::get_instance()->get_close_log()==0) {
        LOG_INFO <<client->getSocket()<<" is close ";
    }
        client->close();
}