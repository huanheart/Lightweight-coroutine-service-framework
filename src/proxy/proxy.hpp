#include<iostream>


using namespace std;


//������������������
//�Լ���ʹ��nginx��ʱ��������Ҫ����http��ʱ���ж��Ƿ���nginx��ʶ�����û�У���ô����ֱ�Ӿܾ����ʣ�������һ��ϵͳѹ��

//���������
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
        ProxyType=0;  //0��ʾʹ��nginx
     }
     ~nginxProxy()
     {
        cout<<"nginxProxy killing"<<endl;
     }
};


//��ʹ��nginx����
class noPorxy :public AbstractProxy
{
public:
    noPorxy(){
        ProxyType=1; //1��ʾ��ʹ���κη������
    }
    ~noPorxy()
    {
        cout<<"noProxy killing "<<endl;
    }
};

//����Ϊ���󹤳�
class AbstractFactory
{
public:
    virtual AbstractProxy* createProxy()=0;   //�������������
    virtual ~AbstractFactory(){}
};

//����nginx�Ĺ�����
class nginxFactory :public AbstractFactory
{
public:
    AbstractProxy* createProxy()
    {
        return new nginxProxy();         //�����������new�����ݣ����û�����
    }
    ~nginxFactory()
    {
        cout<<"nginxFactory kiiling now"<<endl;
    }
};

//����noProxy�Ĺ���
class noPorxyFactory:public AbstractFactory
{
public:
    AbstractProxy* createProxy()
    {
        return new noPorxy();         //�����������new�����ݣ����û�����
    }
    ~noPorxyFactory()
    {
        cout<<"noproxyfactory killing now "<<endl;
    }
};

