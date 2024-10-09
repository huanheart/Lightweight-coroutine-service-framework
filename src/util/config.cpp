#include "config.h"


Config::Config() {
    //端口号默认9006
    PORT = 9006;
    //数据库连接池数量,默认8
    sql_num = 8;
    //线程池内的线程数量,默认8
    thread_num = 8;
    //关闭日志,默认不关闭
    close_log = 0;
    //默认选择使用nginx代理
    Proxy=0;

}
//c++11以后局部懒汉不需要加锁
Config * Config::get_instance()
{
    static Config config;
    return &config;
}


void Config::parse_arg(int argc,char * argv[] )
{
    int opt;
    const char * str="p:l:m:o:s:t:c:a:n:";

    while( (opt=getopt(argc,argv,str) )!=-1 )
    {
        switch(opt)
        {
            case 'p': //自定义端口号
            {
                //optarg 是一个指向当前选项参数的指针，通常用于处理带参数的选项。默认后台会更新这个东西
                PORT = atoi(optarg);
                break;
            }
            case 's':       //数据库连接数量
            {
                sql_num = atoi(optarg);
                break;
            }
            case 't':  //线程数量
            {
                thread_num = atoi(optarg);
                break;
            }
            case 'c':    //是否关闭日志
            {
                close_log = atoi(optarg);
                break;
            }
            case 'n':   //n表示是否用nginx反向代理，默认用，0表示用，1表示不用
            {
                Proxy=atoi(optarg);
                break;
            }
            default:
                break;
        }

    }

}