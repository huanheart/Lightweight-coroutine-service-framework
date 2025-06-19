#include<iostream>
#include <sys/epoll.h>
#include<string>
#include <muduo/base/Logging.h>
#include<muduo/base/AsyncLogging.h>
#include <sys/resource.h> //设置虚拟内存的
#include<sys/stat.h>
#include<unistd.h>

#include"cglmysql/sql_connection_pool.h"
#include"server/http_server.h"
#include"util/config.h"

using namespace std;
//将对应生成的日志放入到logs目录下
//此目录针对的是my_web的exe文件，而不是与main.cpp的位置也不是与可执行文件的相对位置，跳了一个坑
const std::string LOGDIR="../logs/";

// sylar::IOManager::ptr worker=nullptr;
off_t kRollSize = 500*1000*1000;
muduo::AsyncLogging * g_asyncLog=nullptr;

void asyncOutput(const char * msg,int len)
{
    g_asyncLog->append(msg,len);
}


void run(string user,string passwd,string database_name,int sql_num ,int port,int proxy)
{

    Connection_pool * conn_pool=Connection_pool::get_instance();
    sylar::Address::ptr m_adress=sylar::Address::LookupAnyIPAddress("0.0.0.0:"+to_string(port) );

    //数据库连接池
    conn_pool->init("localhost",user,passwd,database_name,3306,sql_num);
    //创建httpserver
//    http_server* httpserver=new http_server();
    // std::shared_ptr<http_server> httpserver = std::make_shared<http_server>(true,worker.get());
    std::shared_ptr<http_server> httpserver = std::make_shared<http_server>(true);
    ////一定要在调取器创建之后之后new且用智能指针初始化，不能直接new
    /// 否则将会出错terminate called after throwing an instance of 'std::bad_weak_ptr'
    httpserver->m_user=user;
    httpserver->m_password=passwd;
    httpserver->m_database_name=database_name;
    httpserver->initmysql_result(conn_pool);
    httpserver->initproxy(proxy);
    //启动bind函数，用于进行监听服务端socket
    while(!httpserver->bind(m_adress,false) ){
        sleep(1);
    }
    cout<<"http start!!!"<<endl;
    httpserver->start();
}

int main(int argc,char * argv[] )
{
    {
        // 设置最大的虚拟内存为2GB
        size_t kOneGB = 1000*1024*1024;
        rlimit rl = { 2*kOneGB, 2*kOneGB };
        setrlimit(RLIMIT_AS, &rl);
    }
    //命令行解析
    Config::get_instance()->parse_arg(argc,argv);

    if(access(LOGDIR.c_str(),F_OK)==-1){
        mkdir(LOGDIR.c_str(),0755); //创建目录
    }

    char name[256]={'\0'};
    std::string logFileName = LOGDIR + ::basename(name);
    muduo::AsyncLogging log(logFileName.c_str(), kRollSize);
    log.start();
    g_asyncLog = &log;
    muduo::Logger::setOutput(asyncOutput);
    string user="root";
    string passwd="123456";
    string database_name="webserver";
    // worker.reset(new sylar::IOManager(1,false) );
    sylar::IOManager manager(Config::get_instance()->get_thread_num(),true);
    manager.scheduleLock([=]() {
        run(user, passwd, database_name, Config::get_instance()->get_sql_num(), Config::get_instance()->get_port(), Config::get_instance()->get_proxy());
    });

    return 0;
}


