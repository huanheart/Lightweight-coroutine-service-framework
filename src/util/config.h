#ifndef CONFIG_H
#define CONFIG_H

#include <stdio.h>
#include<unistd.h>
#include<errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include<cassert>


class Config
{
public:
    void parse_arg(int argc,char * argv[] );
    static Config * get_instance();

private:
    Config();
    ~Config(){};

public:
    int get_port()const {
        return PORT;
    }
    int get_proxy() const {
        return Proxy;
    }
    int get_sql_num()const {
        return sql_num;
    }
    int get_thread_num()const {
        return thread_num;
    }
    int get_close_log()const {
        return close_log;
    }
private:
    //端口号
    int PORT;
    //默认选择nginx,固使用反向代理
    int Proxy;
    //数据库连接池数量
    int sql_num;

    //线程池内的线程数量
    int thread_num;

    //是否关闭日志
    int close_log;



};


#endif