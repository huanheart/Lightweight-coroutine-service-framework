#include<iostream>


using namespace std;


//可以设置最大的连接数
//以及当使用nginx的时候，我们需要解析http的时候判断是否有nginx标识，如果没有，那么我们直接拒绝访问，缓解了一定系统压力

//抽象代理类
class AbstractProxy
{
public:
    int ProxyType=-1;
    virtual ~AbstractProxy(){}
};



class nginxProxy:public AbstractProxy
{
public:
     nginxProxy()
     { 
        ProxyType=0;  //0表示使用nginx
     }
     ~nginxProxy()
     {
        cout<<"nginxProxy killing"<<endl;
     }
};


//不使用nginx的类
class noPorxy :public AbstractProxy
{
public:
    noPorxy(){
        ProxyType=1; //1表示不使用任何反向代理
    }
    ~noPorxy()
    {
        cout<<"noProxy killing "<<endl;
    }
};

//该类为抽象工厂
class AbstractFactory
{
public:
    virtual AbstractProxy* createProxy()=0;   //创建抽象代理类
    virtual ~AbstractFactory(){}
};

//生产nginx的工厂类
class nginxFactory :public AbstractFactory
{
public:
    AbstractProxy* createProxy()
    {
        return new nginxProxy();         //不负责处理这个new的内容，让用户负责
    }
    ~nginxFactory()
    {
        cout<<"nginxFactory kiiling now"<<endl;
    }
};

//生成noProxy的工厂
class noPorxyFactory:public AbstractFactory
{
public:
    AbstractProxy* createProxy()
    {
        return new noPorxy();         //不负责处理这个new的内容，让用户负责
    }
    ~noPorxyFactory()
    {
        cout<<"noproxyfactory killing now "<<endl;
    }
};

