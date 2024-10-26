#ifndef _CONNECTION_POOL_
#define _CONNECTION_POOL_

#include<stdio.h>
#include<list>
#include<error.h>
#include<string>
#include<string.h>
#include<iostream>
#include<mysql/mysql.h>
#include<mutex>

#include"../util/locker.h"

class Connection_pool
{

private:
    Connection_pool()=default; //�����ǵ����࣬��Ȼ��ô��
    ~Connection_pool();

    int m_max_conn; //���������
    int m_cur_conn=0;  //��ǰ�Ѿ�ʹ�õ�������
    int m_free_conn=0; //��ǰ���е�������
    std::mutex lock;

    std::list<MYSQL*> conn_list; //���ӳ�
    Sem reserve;

public:
    std::string m_url;  //������ַ   ��������Զ���������ݿ�
    std::string m_port; //���ݿ�˿ں�
    std::string m_user; //��¼���ݿ��û���
    std::string m_passwd; //����
    std::string m_databasename; //ʹ�����ݿ���


public:

    MYSQL*get_connection(); //��ȡ���ݿ�����
    bool release_connection(MYSQL*conn); //�ͷ�����
    int get_free_conn(); //��ȡ����
    void destroy_pool();      //������������

    static Connection_pool *get_instance();
	void init(std::string url, std::string user, 
			std::string passwd, std::string data_base_name,
			int port, int max_conn);


};


//�������Ҫ��������RAII˼��ģ�����һ���м��࣬ʹ���߿��Բ��ù������ݿ����ӳصĵײ�ϸ��
class connectionRAII{

public:
	connectionRAII(MYSQL **con, Connection_pool *connPool);
	~connectionRAII();
	
private:
	MYSQL *conRAII;
	Connection_pool *poolRAII;
};

#endif
