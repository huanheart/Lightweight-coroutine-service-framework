#ifndef HTTPCONNECTION_H
#define HTTPCONNECTION_H
#include <unistd.h> //��fork������Щ����
#include <signal.h>
#include <sys/types.h> //�����������pid_t��size_t��ssize_t�����͵Ķ��壬��Щ������ϵͳ����о������ڱ�ʾ����ID����С���з��Ŵ�С�ȡ�
#include <sys/epoll.h>
#include <fcntl.h> //���ڶ��Ѵ򿪵��ļ����������и��ֿ��Ʋ������������ļ���������״̬��־��������I/O�ȡ�
#include <sys/socket.h>
#include <netinet/in.h> //Internet��ַ�����ؽṹ��ͷ��ų���,��������Ľṹ����sockaddr_in�����ڱ�ʾIPv4��ַ�Ͷ˿ںš�������������
#include <arpa/inet.h>  //������һЩ��IPv4��ַת����صĺ���
#include <assert.h>     //��ͷ�ļ�������һ����assert()�������ڳ����в������
#include <sys/stat.h>   //���ڻ�ȡ�ļ���״̬��Ϣ�����ļ��Ĵ�С������Ȩ�޵ȡ�
#include <string.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <stdarg.h>
#include <errno.h>
#include <sys/wait.h> //��ͷ�ļ�������һЩ����̵ȴ���صĺ����ͺ꣬������õĺ�����waitpid()�����ڵȴ�ָ�����ӽ���״̬�����仯
#include <sys/uio.h>
// ��ͷ�ļ�������һЩ��I/O����������صĺ����ͽṹ�塣
// ������õĽṹ����iovec����������һ��ϵͳ�����д������������ڴ����������
// ��������������ʹ��readv()��writev()���������з�ɢ��ȡ�;ۼ�д�롣
#include <map>

#include "../cglmysql/sql_connection_pool.h"
#include"../my_stl/my_stl.hpp"
#include"../proxy/proxy.hpp"
#include"../util/socket.h"


class http_conn
{

public:
    static const int FILENAME_LEN = 200;       // �ļ�������
    static const int READ_BUFFER_SIZE = 2048;  // ����Ļ�������С
    static const int WRITE_BUFFER_SIZE = 1024; // д��Ļ�������С
    enum METHOD
    {
        GET = 0,
        POST,
        HEAD,
        PUT,
        DELETE,
        TRACE,
        OPTIONS,
        CONNECT,
        PATH
    };
    enum CHECK_STATE
    {
        CHECK_STATE_REQUESTLINE = 0,
        CHECK_STATE_HEADER,
        CHECK_STATE_CONTENT
    };
    enum HTTP_CODE
    {
        NO_REQUEST,
        GET_REQUEST,
        BAD_REQUEST,
        NO_RESOURCE,
        FORBIDDEN_REQUEST,
        FILE_REQUEST,
        INTERNAL_ERROR,
        CLOSED_CONNECTION
    };
    enum LINE_STATUS
    {
        LINE_OK = 0,
        LINE_BAD,
        LINE_OPEN
    };

public:

    http_conn() = default;
    ~http_conn()=default;
    void delete_proxy(); 
    //���������Ӧ�����Լ�����ɾ����������������������ɾ������Ϊÿ��http����һ���û�
    //һ���û���ɾ���ˣ������������û�����Ҫ���Ĺ�������Ȼ������webserver�����ˣ��Ž��������һ��ɾ����������Ҫ


public:
    void init(sylar::Socket::ptr &client, char *, int, std::string user, std::string passwd, std::string sqlname,bool keepalive);
    void close_conn(bool real_close = true);
    bool process();
    bool read_once();
    bool write();

    sockaddr_in *get_address()
    {
        return &m_address;
    }
    void initmysql_result(Connection_pool *conn_pool);
    void initproxy(int decideproxy);
    int timer_flag;
    int improv;

public:
    static int m_user_count;
    MYSQL *mysql;
    int m_state; // ��Ϊ0��дΪ1,����״̬��
private:
    void init();
    HTTP_CODE process_read();                               // ���̶�ȡ
    bool process_write(HTTP_CODE ret);                      // ����д��
    HTTP_CODE parse_request_line(char *text);               // ����������
    HTTP_CODE parse_headers(char *text,bool &decide_proxy);    //��������˴����࣬���Խ�bool�ĳ�int      // ����ͷ��
    HTTP_CODE parse_content(char *text);                    // ��������
    HTTP_CODE do_request(bool &decide_proxy);                                 // ������
    char *get_line() { return m_read_buf + m_start_line; }; // ��ȡ��
    LINE_STATUS parse_line();                               // ������
    void unmap();

    // ����Ӧ�úͷ����йأ�����ͽ��յ���Ӧ�й�

    bool add_response(const char *format, ...); // ������Ӧ
    bool add_content(const char *content);
    bool add_status_line(int status, const char *title);
    bool add_headers(int content_length);
    bool add_content_type();
    bool add_content_length(int content_length);
    bool add_linger();
    bool add_blank_line();

public:       //������Ҫ��Ϊ˽��
    sylar::Socket::ptr m_client; //�׽��ֵ��ļ���������Ӧ�ķ�װ
    sockaddr_in m_address;
    char m_read_buf[READ_BUFFER_SIZE];
    long m_read_idx; // ��ȡ�����±�
    long m_checked_idx;
    int m_start_line;
    char m_write_buf[WRITE_BUFFER_SIZE];
    int m_write_idx;
    CHECK_STATE m_check_state; // ��ǰ���״̬������ͷ���������
    METHOD m_method;           // ����һ����������
    char m_real_file[FILENAME_LEN];
    char *m_url;
    char *m_version;
    char *m_host;
    long m_content_length;
    bool m_linger;
    char *m_file_address; // �ļ��������ĵ�ַ�ɣ�
    struct stat m_file_stat;
    struct iovec m_iv[2];
    int m_iv_count;
    int cgi;        // �Ƿ����õ�post
    char *m_string; // �洢����ͷ����
    int bytes_to_send;
    int bytes_have_send;
    char *doc_root;

    int m_TRIGMode; // ����ģʽ

    char sql_user[100];
    char sql_passwd[100];
    char sql_name[100];


};

#endif
