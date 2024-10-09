#include <stdio.h>
#include <string>
#include <string.h>
#include <stdlib.h>
#include <list>
#include <pthread.h>
#include <iostream>
#include<muduo//base/Logging.h>
#include "sql_connection_pool.h"
#include"../util/config.h"

//c++11以后，局部静态懒汉不需要加锁
Connection_pool* Connection_pool::get_instance()
{
    static Connection_pool conn_pool;
    return &conn_pool;
}

void Connection_pool::init(std::string url, std::string user,std::string passwd, std::string data_base_name,int port, int max_conn)
{
	m_url = url; //主机地址
	m_port = port;   //端口号这些
	m_user = user;
	m_passwd = passwd;
	m_databasename = data_base_name;
    //现在是往连接池放连接

    for(int i=0;i<max_conn;i++)
    {        
        MYSQL*con=nullptr;
        con=mysql_init(con);  //初始化而已，还没有连接
        if(con==nullptr)
        {
            //直接输出这个错误日志，告诉他初始化的时候出错了,可能有端口号这些的原因
            if(Config::get_instance()->get_close_log()==0) {
                LOG_INFO << "MYSQL ERROR";
            }
            exit(1);
        }
        //获得句柄：真正得到这个空间
       con = mysql_real_connect(con, url.c_str(), user.c_str(), passwd.c_str(), data_base_name.c_str(), port, nullptr, 0);
       if(con==nullptr)
       {
           if(Config::get_instance()->get_close_log()==0) {
               LOG_INFO << "MYSQL ERROR";
           }
            exit(1);
       }
       conn_list.push_back(con); //放入一个连接
       ++m_free_conn; //因为还没有用到刚刚才放进去的这个连接，固然在池子里面的连接多了一个
    }
    reserve=Sem(m_free_conn); //信号量初始化，匿名对象给到拷贝，被创建即被析构
    m_max_conn=m_free_conn; //m_free_conn这个和刚开始的最大连接数是一样的，因为在内部将这个空闲的给++了
}


//当有请求时，从数据库连接池中返回一个可用连接，更新使用和空闲连接数
MYSQL * Connection_pool::get_connection()
{
    MYSQL *con=nullptr;
    if(conn_list.size()==0)
        return nullptr;
    reserve.wait(); //信号量去阻塞等待，其他用户返回回来的连接

    lock.lock();
    con=conn_list.front();
    conn_list.pop_front();
    --m_free_conn;
    ++m_cur_conn;
    lock.unlock();
    return con;
}


//释放当前连接
bool Connection_pool::release_connection(MYSQL*con)
{
    if(con==nullptr)
        return false;
    lock.lock();
    
    conn_list.push_back(con); //回归到池子里
    ++m_free_conn;
    --m_cur_conn;
    lock.unlock();

    reserve.post();
    //使得这个信号量+1，因为到时候肯定会有多个线程访问这个单例类的
    //固然其释放和增加的函数肯定都会有线程调用的,即多个客户端访问一个服务器的线程，但是真的要开一个线程为客户去服务吗?
    //其实没有必要的,可以使用协程
    return true;

}

//销毁数据库连接池的所有连接

void Connection_pool::destroy_pool()
{
    lock.lock();
    if(conn_list.size()>0)
    {
        std::list<MYSQL*>::iterator it;   //内部维护应该是个二级指针了，根据模板对应内容来维护一个指向参数的指针
        for(it=conn_list.begin();it!=conn_list.end();++it)
        {
            MYSQL*con=*it;
            mysql_close(con);
        }
        m_cur_conn=0;
        m_free_conn=0;
        conn_list.clear();
    }
    lock.unlock();
}

int Connection_pool::get_free_conn()
{
    return this->m_free_conn;
}

Connection_pool::~Connection_pool() //类似rall的思想
{
    destroy_pool();
}


connectionRAII::connectionRAII(MYSQL**con,Connection_pool*conn_pool)
{
    *con=conn_pool->get_connection();
    conRAII=*con;
    poolRAII=conn_pool;

}

connectionRAII::~connectionRAII()
{
    poolRAII->release_connection(conRAII);
}
