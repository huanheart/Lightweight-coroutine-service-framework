#include "http_conn.h"

#include <mysql/mysql.h>
#include <fstream>
#include<mutex>
#include<muduo/base/Logging.h>
#include"../CoroutineLibrary/ioscheduler.h"
#include"../CoroutineLibrary/hook.h"
#include"../util/config.h"

const char *ok_200_title = "OK";
const char *error_400_title = "Bad Request";
const char *error_400_form = "Your request has bad syntax or is inherently impossible to staisfy.\n";
const char *error_403_title = "Forbidden";
const char *error_403_form = "You do not have permission to get file form this server.\n";
const char *error_404_title = "Not Found";
const char *error_404_form = "The requested file was not found on this server.\n";
const char *error_500_title = "Internal Error";
const char *error_500_form = "There was an unusual problem serving the request file.\n";

std::mutex m_lock;
//FindCache<std::string,MemoryPool<std::string> > users;
//�滻Ϊ��Ȩ����LRU���ݽṹ
LRUCache<std::string,std::string,MemoryPool<std::string> > users;

//��ע�⣬������ܷ��뵽http_conn.h�У���Ϊÿ���û���Ӧ�˺ܶ��http�࣬������ȫ�ֵ�users�����й���
//���Ƿ��뵽����Ϳ�������ȫ�ֵ�һ��������(�������û������ݶ��洢������ط�)

AbstractFactory * factory=nullptr;    //��������ʱ�������Ҫ����
AbstractProxy * obj=nullptr;


void http_conn::delete_proxy()
{
    if(factory!=nullptr){
        delete factory;
        factory=nullptr;
    }
    if(obj!=nullptr){
        delete obj;
        obj=nullptr;
    }
}

void http_conn::initmysql_result(Connection_pool *conn_pool)
{
    //�����ӳ������ȡһ������
    MYSQL* mysql=nullptr;
    connectionRAII mysqlcon(&mysql,conn_pool);
    //��user���м���username,passwd���ݣ����Ƿ�������㲻���������
    if(mysql_query(mysql,"select username,passwd from user") ) //������ط�0��˵���ڲ�����
    {
        if(Config::get_instance()->get_close_log()==0) {
            LOG_INFO << "select error: " << mysql_error(mysql);
        }
    }

    //�ӱ��м��������Ľ������
    MYSQL_RES *result=mysql_store_result(mysql);

    //���ؽ����������
    int num_fields=mysql_num_fields(result);

    //���������ֶνṹ������
    MYSQL_FIELD * fields=mysql_fetch_fields(result); //��������ʲô���𣿶���ֲ�����Ҳû������ɶ���ó���

    while(MYSQL_ROW row=mysql_fetch_row(result) )
    {
        std::string temp1(row[0]);
        std::string temp2(row[1]);
        users.push(temp1,temp2);  //��ŵ���Ӧ���ڴ��У���ʵ����Ūһ��redis����ŵ�����
    }
}

void http_conn::initproxy(int decideproxy)
{
    //ʹ�ù���ģʽ��������Ӧ�����Լ�������
    switch(decideproxy)
    {
        case 0:
            factory=new nginxFactory();
            break;
        case 1:
            factory=new noPorxyFactory();
            break;
        default:
            break;
    }
    obj=factory->createProxy();
}



int http_conn::m_user_count=0; //��̬��Ա������ʼ��

//�ر����ӣ��ͻ�����-1
//���Է��֣���ʱ�ر������ӣ������ƺ�û�йرյ�ǰ��ʱ����
//�������Ƿ����bug
void http_conn::close_conn(bool real_close)
{
    if(real_close&&(m_client->getSocket()!=-1) )
    {
        printf("close %d\n",m_client->getSocket());
        m_user_count--;
    }

}


//��ʼ�����ӣ��ⲿ���ó�ʼ���׽��ֵ�ַ
void http_conn::init(sylar::Socket::ptr &client,char * root,int TRIGMode,
                     std::string user,std::string passwd,std::string sqlname,bool keepalive)
{
    this->m_client=client;
    m_user_count++;
    m_linger=keepalive;
 //�������������������ʱ����������վ��Ŀ¼�����http��Ӧ��ʽ������߷��ʵ��ļ���������ȫΪ�� //���ﲻ��
    doc_root=root; //���Ӧ���Ƿ������ĸ�Ŀ¼
    //(�üӵ�config���棩
    m_TRIGMode=TRIGMode; //
    // (�üӵ�config���棩
    strcpy(sql_user, user.c_str());
    strcpy(sql_passwd, passwd.c_str());
    strcpy(sql_name, sqlname.c_str());
    init(); //�ڲ����ó�ʼ��
}

//��ʼ���½��յ�����
//check_stateĬ��Ϊ����������״̬
void http_conn::init()
{
    mysql=nullptr; 
    bytes_to_send = 0;
    bytes_have_send = 0;
    m_check_state = CHECK_STATE::CHECK_STATE_REQUESTLINE;
    m_method = METHOD::GET;
    m_url = 0;
    m_version = 0;
    m_content_length = 0;
    m_host = 0;
    m_start_line = 0;
    m_checked_idx = 0;
    m_read_idx = 0;
    m_write_idx = 0;
    cgi = 0;
    m_state = 0;
    timer_flag = 0;
    improv = 0;
    memset(m_read_buf, '\0', READ_BUFFER_SIZE);
    memset(m_write_buf, '\0', WRITE_BUFFER_SIZE);
    memset(m_real_file, '\0', FILENAME_LEN);

}


//��״̬����ȡ״̬�����ڷ�����һ������
//����ֵΪ�еĶ�ȡ״̬����LINE_OK,LINE_BAD,LINE_OPEN
http_conn::LINE_STATUS http_conn::parse_line() //���ڽ�����ͬ��״̬������ÿһ�У�
{
    char temp;
    for (; m_checked_idx < m_read_idx; ++m_checked_idx)         //m_read_idx��һ��ʼ��ˮƽ�������߱�Ե������ȡ���������ݵĳ��ȣ������Ǵ�ŵ�m_read_buf�еģ�ͨ��webserver����read_once
    {
        temp = m_read_buf[m_checked_idx];
        if(temp=='\r') //�س�����
        {
            if( (m_checked_idx+1) ==m_read_idx) {
                return LINE_OPEN;
            }
            else if(m_read_buf[m_checked_idx+1]=='\n')
            {
                m_read_buf[m_checked_idx++]='\0';
                m_read_buf[m_checked_idx++]='\0';
                return LINE_OK;
            }
        }else if(temp=='\n'){
            if(m_checked_idx>1&&m_read_buf[m_checked_idx-1] =='\r')  //-1��+1��Ŀ���Ƿ�ֹ���
            {
                m_read_buf[m_checked_idx-1]='\0';
                m_read_buf[m_checked_idx++]='\0';
                return LINE_OK;        
            }
            return  LINE_BAD;
        }
    }
    return LINE_OPEN;

}

//ѭ����ȡ�ͻ����ݣ�ֱ�������ݿɶ���Է��ر�����
//������ET����ģʽ�£���Ҫһ���Խ����ݶ���
//process_read ���� read_once �ṩ�����ݣ�read_once �����ݴ� socket �ж�ȡ����������Ȼ�� process_read �ڻ������н������ݡ�
//read_once ֻ�������ݶ�ȡ���� process_read �������ݽ���,���߿��������������ݶ�ȡ������
bool http_conn::read_once()
{
    if(m_read_idx>=READ_BUFFER_SIZE)
        return false;
    int bytes_read = 0;

    //LT��ȡ����(ˮƽ����)
    if(m_TRIGMode==0)
    {
        bytes_read=m_client->recv(m_read_buf+m_read_idx,READ_BUFFER_SIZE-m_read_idx,0);
        m_read_idx+=bytes_read;

        if(bytes_read<=0)
            return false;
        return true;
    }
    //ET��ȡ����
    else{
        //�����������Ҫһ���Զ�ȡ�����ݵģ���Ȼ��ѭ��etģʽ�ͷ�ѭ��etģʽ��
        while(true)
        {
            bytes_read=m_client->recv(m_read_buf + m_read_idx, READ_BUFFER_SIZE - m_read_idx, 0);
            if(bytes_read==-1)
            {
                if(errno==EAGAIN || errno==EWOULDBLOCK) //˵������û�������ˣ���Ȼ�˳�ѭ��
                    break;
                return false;  //return false˵���ڲ�������
            }
            else if(bytes_read==0) //˵��������û�ж�������
            {
                return false;
            }
            m_read_idx+=bytes_read;
           
        }   
        return true;
    }

}

//����http�����У�������󷽷���Ŀ��url�Լ��汾��
//���������������Ҫ���õ�����ĳ�Ա�ȴ��������ˣ�Ȼ�������õ�
//�����е���˼���鿴��ȸ�ʼ�
http_conn::HTTP_CODE    http_conn::parse_request_line(char* text)
{
    m_url=strpbrk(text," \t"); //���س���������ڶ����ַ��������еĵ�һ�������ַ���1���±�ĺ��������
    if(!m_url)
    {   
        return BAD_REQUEST;
    }
    *m_url++='\0';
    char * method=text;

    if(strcasecmp(method,"GET")==0) //���ڱȽϵĺ���
        m_method=GET;
    else if(strcasecmp(method,"POST")==0)
    {
        m_method=POST;
        cgi=1; //���cgi����������ģ��Ƿ����õ�post
    }
    else {
     
        return BAD_REQUEST; ///�����û�����������������Ŀֻ������get��post
    }

    m_url+=strspn(m_url," \t" );  //���忴��ȸ
    


    m_version=strpbrk(m_url," \t");
    if (!m_version){
        
        return BAD_REQUEST;
    }
    *m_version++ = '\0';
    
    m_version += strspn(m_version, " \t");

    if (strcasecmp(m_version, "HTTP/1.1") != 0) //��������
    {
   
        return HTTP_CODE::BAD_REQUEST;
    }

    if(strncasecmp(m_url,"http://",7)==0) //�ж������ַ�����ǰ7���ַ��Ƿ����
    {
        m_url+=7;
        m_url=strchr(m_url,'/'); //�������m_url��http://�����Ѿ�û���ˣ���Ȼ�����ˣ������ȽϺú�����ж�
    }


    if (strncasecmp(m_url, "https://", 8) == 0)
    {
        m_url += 8;
        m_url = strchr(m_url, '/');
    }

    if(!m_url || m_url[0]!='/' ) //˵�������˴����ˣ����ܺ���û�������ˣ�����û�н�ȡ��'/'
    {
        return BAD_REQUEST;
    }
    //��urlΪ/ʱ����ʾ�жϽ���         Ϊʲô��ʱҪ
    if(strlen(m_url)==1)
        strcat(m_url,"judge.html");
    m_check_state= CHECK_STATE_HEADER;
    return NO_REQUEST;
}

//����http�����һ��ͷ����Ϣ(���н���)
//��������ݻᱻѭ������
http_conn::HTTP_CODE  http_conn::parse_headers(char *text,bool &decide_proxy)
{
    if(text[0]=='\0')
    {
        if(m_content_length!=0)
        {
            m_check_state=CHECK_STATE_CONTENT;
            return NO_REQUEST;
        }
        return GET_REQUEST;
    }
    else if(strncasecmp(text,"Connection:",11)==0) 
    {
        text+=11;
        text+=strspn(text," \t");
        ////��ɾ�����ݿͻ��˵���Ը�������Ƿ�Ϊ�����Ƕ�����(�ĳɷ��������ѡ��
//        if(strcasecmp(text,"keep-alive")==0)
//        {
//            m_linger=true;
//        }
    }
    else if (strncasecmp(text, "Content-length:", 15) == 0)
    {
        text+=15;
        text+=strspn(text," \t");
        m_content_length=atol(text); //��ȡ����ַ����ĳ���
    }
    else if(strncasecmp(text,"Host:",5)==0 )   //�Ƚ�text�ַ���ǰ5���ַ���ɵ��ַ����Ƿ���Host:ƥ��
    {
        text+=5;
        text+=strspn(text," \t"); //��������Ϊ��ͷ
        m_host=text;             //��ȡ������
    }
    else if(strncasecmp(text,"X-Forwarded-By:",15)==0 ) //˵����nginxת����
    {
        if(obj->ProxyType==1) //���û��ѡ��nginx���������Ȼ��nginx���ʹ��������ݽ�������
        {
            return BAD_REQUEST;
        }
        decide_proxy=false; //Ϊ���������жϵ�ʱ���õ��ģ��ж���nginx��ʱ���Ƿ����˶�Ӧ����nginxģʽ��
        
    }
    else
    {
        //�������־ϵͳ���鿴ͷ����ʲô����ı�ʶ�������Ƿ��ͷ��Լ������������һЩ��ʶ
        if(Config::get_instance()->get_close_log()==0) {
            LOG_INFO << "oop!unknow header: " << text;
        }
    }
    
    return NO_REQUEST;
}


//�ж�http�����Ƿ���������
http_conn::HTTP_CODE http_conn::parse_content(char * text)
{
    if(m_read_idx>=(m_content_length+m_checked_idx) )
    {
        text[m_content_length]='\0';
        //post���������Ϊ������û���������
        m_string=text;
        return GET_REQUEST;
    }
    return NO_REQUEST;
}


//process_read �Ǵ��� HTTP Э��ĺ����߼������������ж������Ӧ
//process_read ���� read_once �ṩ������
http_conn::HTTP_CODE  http_conn::process_read()
{
    LINE_STATUS line_status=LINE_OK;
    HTTP_CODE ret=NO_REQUEST;
    char * text=0;
    //�ж����������
    bool decide_proxy=true; 
    //Ĭ����û��nginx(�������һ������Ҫ�ֲ������ģ�������ȫ�ֱ���)
    //���������ȫ�ֵĴ��󣨼���һ��nginx�������������ô��ʱ������������false����ô����һ��9006�˿ڣ��ͻ�����Ϊ��nginx��������ģ�

    //���ѭ��һ��ʼĬ���ǽ��� ||�ұߵ��Ǹ� ( (line_status=parse_line() )==LINE_OK������Ū��֮��ͻῪʼ����ͷ�Լ�������Ĵ�����
    while( (m_check_state==CHECK_STATE_CONTENT &&line_status==LINE_OK)
          || ( (line_status=parse_line() )==LINE_OK) ) //������ǰ��״̬�Ƿ�ʹ�ʱ���õ�line_status״̬һ��
    {
        text=get_line();

        m_start_line=m_checked_idx;
        if(Config::get_instance()->get_close_log()==0) {
            LOG_INFO << text;
        }
        switch (m_check_state)
        {
            case CHECK_STATE_REQUESTLINE:
            {
                ret=parse_request_line(text);
                if(ret==BAD_REQUEST){
                    return BAD_REQUEST;       //����˵���ܴ���put��Щ���������������BAD_REQUEST
                }
                break;
            }
            case CHECK_STATE_HEADER:
            {
                ret=parse_headers(text,decide_proxy);
                if(ret==BAD_REQUEST)
                    return BAD_REQUEST;
                else if(ret==GET_REQUEST)
                    return do_request(decide_proxy);
                break;
            }
            case CHECK_STATE_CONTENT :
            {
                ret=parse_content(text);
                if(ret==GET_REQUEST) //˵����һ��������GET����
                    return do_request(decide_proxy);
                line_status=LINE_OPEN;
                break;
            }
            default:
                return INTERNAL_ERROR; //�������ڲ�����

        }

    }
    if(decide_proxy==false){ 
        if(obj->ProxyType==1) 
            return BAD_REQUEST;
    }else {
        if(obj->ProxyType==0)
            return BAD_REQUEST;
    }
    return NO_REQUEST;

}

http_conn::HTTP_CODE http_conn::do_request(bool & decide_proxy)
{

    if(decide_proxy==false){ //��ʾ����nginx����ô�鿴obj�Ƿ�Ϊ1(1����û����)
        if(obj->ProxyType==1) //��Ȼì��
            return BAD_REQUEST;
    }else {
        if(obj->ProxyType==0){
            return BAD_REQUEST;
        }
    }

    strcpy(m_real_file,doc_root); //���ļ��ĸ�Ŀ¼��Ȼ��ֵ��m_real_file����
    int len=strlen(doc_root);
    const char * p=strrchr(m_url,'/'); //�����ֵ���ܱ�,����'/'��һ�γ��ֵ�λ��

    //����cgi���ж�����������ʽ
    if(cgi==1&&(*(p+1)=='2' || *(p+1)=='3' ) )
    {
        //���ݱ�־�ж��ǵ�¼��⻹��ע����
        char flag=m_url[1];
        char *m_url_real=(char *)malloc(sizeof(char) *200);
        strcpy(m_url_real, "/");
        strcat(m_url_real,m_url+2); //��m_url+2������ַ�����ӵ�m_url_real��
        strncpy(m_real_file+len,m_url_real,FILENAME_LEN-len-1); 
        //��m_url_real���ַ�������ǰ����������Ȼ�������һ���ַ�����
        free(m_url_real); //�ͷŲ���
    
      //���û�����������ȡ����
    char name[100],password[100];
    
    {
        int i;
        for(i=5;m_string[i]!='&';++i)
            name[i-5]=m_string[i];
        name[i-5]='\0';
        int j=0;
        for(i=i+10;m_string[i]!='\0';++i,++j)
            password[j]=m_string[i];
        password[j]='\0';

    }
    if(*(p+1)=='3')
    {
        //�����ע�ᣬ�ȼ�����ݿ����Ƿ���������
        //û�������������
        char * sql_insert=(char*)malloc(sizeof(char)*200);
        strcpy(sql_insert, "INSERT INTO user(username, passwd) VALUES(");
        strcat(sql_insert, "'");
        strcat(sql_insert, name);
        strcat(sql_insert, "', '");
        strcat(sql_insert, password);
        strcat(sql_insert, "')");
        //������һ����Ҫ��ƴ��һ��sql������
//        BackNode<std::string> back;
//        back=users.find(name);
        pair<bool,std::string> back=users.get(name);
        if(back.first==false) //˵��û���ҵ�,�û���ά����һ��map��
        {
            m_lock.lock();
	    // std::cout<<"test mysql address "<<mysql<<std::endl;
            int res=mysql_query(mysql,sql_insert); //�������ӵ����ݿ������Ƿ��м�¼
	
	    users.push(name,password);
            m_lock.unlock();
            if(!res) //˵��û�У���ôֱ������Ū����¼���棬�Ƚ���һ����ֵ
                strcpy(m_url,"/log.html");
            else 
                strcpy(m_url, "/registerError.html");
        }
        else 
            strcpy(m_url, "/registerError.html");
        //����ǵ�¼��ֱ���ж�
        //���������������û����������ڱ��п��Բ��ҵ�������1�����򷵻�0
    }
      else if (*(p + 1) == '2')
      {
//        BackNode<std::string> back;
        pair<bool,std::string> back=users.get(name);

        if(back.first==true && back.second==password)
            strcpy(m_url, "/welcome.html");
        else 
            strcpy(m_url, "/logError.html");
      }

    }
//�����2����3�����¼�Լ�ע��
//����ľ�������״̬�Ľ�����

    if(*(p+1)=='0')
    {
        char *m_url_real=(char*)malloc(sizeof(char)*200 );
        strcpy(m_url_real, "/register.html");
        strncpy(m_real_file+len,m_url_real,strlen(m_url_real) );
        free(m_url_real);

    }
    else if(*(p+1)=='1')
    {
        char * m_url_real=(char*)malloc(sizeof(char)*200 );
        strcpy(m_url_real,"/log.html");
        strncpy(m_real_file + len, m_url_real, strlen(m_url_real));
        free(m_url_real);
    }
    else if(*(p+1)=='5')
    {
        char *m_url_real = (char *)malloc(sizeof(char) * 200);
        strcpy(m_url_real, "/picture.html");
        strncpy(m_real_file + len, m_url_real, strlen(m_url_real));

        free(m_url_real);
    }
    else if(*(p+1)=='6')
    {
        char *m_url_real = (char *)malloc(sizeof(char) * 200);
        strcpy(m_url_real, "/video.html");
        strncpy(m_real_file + len, m_url_real, strlen(m_url_real));

        free(m_url_real);
    }
    else if(*(p+1)=='7')
    {
        char *m_url_real = (char *)malloc(sizeof(char) * 200);
        strcpy(m_url_real, "/fans.html");
        strncpy(m_real_file + len, m_url_real, strlen(m_url_real));
        free(m_url_real);

    }
    else 
        strncpy(m_real_file+len,m_url,FILENAME_LEN-len-1);
    ////stat ������������ȡ���ļ�״̬��Ϣ��䵽����ṹ�����ļ����ͣ��糣���ļ���Ŀ¼�ȣ�
    ////�ļ�Ȩ��
    ////�ļ���С
    ////�������ʱ�䡢�޸�ʱ�䡢����ʱ���
    if(stat(m_real_file,&m_file_stat)<0)  //�������ȡ�ļ���ǰ����Ϣ,stat()������ȡ��Ӧλ�õ��ļ���Ϣ
    {
        return NO_RESOURCE; //û����Դ
    }
    ////������ɶ����򷵻�
    if(!(m_file_stat.st_mode)&S_IROTH) {
        return FORBIDDEN_REQUEST; //��ֹ�����״̬��
    }
    ////�жϵ�ǰ�ļ�·���ǲ���Ŀ¼
    if(S_ISDIR(m_file_stat.st_mode) )
        return BAD_REQUEST;

    int fd=open(m_real_file,O_RDONLY); //ֻ���ķ�ʽ������ļ���Ȼ��������linux�£�����һ���ļ�������������ļ�����Ҫ���͵��ļ��ˣ�
        ////  MAP_PRIVATE ��ӳ�������д����������һ��ӳ���ļ��ĸ��ƣ���˽�˵ġ�д��ʱ���ơ���copy on write���Դ����������κ��޸Ķ�����д��ԭ�����ļ�����
        /// ////m_file_addressΪϵͳ����ӳ����Ǹ��ڴ��ַ
    m_file_address=(char*)mmap(0,m_file_stat.st_size,PROT_READ,MAP_PRIVATE,fd,0); //��ӳ�䣬���忴��ȸ(�����Ҫ�������Ƶ�ļ��������)(��Ҳ���������ڴ湲��)
    //������Ҫ�ǽ�һ����ͨ�ļ�ӳ�䵽�ڴ��У�ͨ������Ҫ���ļ�����Ƶ����дʱʹ��
    ::close(fd);
    
    return FILE_REQUEST;

}

//������溯������������Ǹ�ӳ��
void http_conn::unmap()
{
    if(m_file_address)
    {
        munmap(m_file_address,m_file_stat.st_size);
        m_file_address=0;
    }

}


//write ����ʵ�ʽ����ݷ��͸��ͻ��ˣ��������еĴ��󡢽��ȸ��µȲ�����
bool http_conn::write()
{
    int temp=0;
    if(bytes_to_send==0) //�����ֽ���==0
    {
        //�ĳɹ�ע���¼�
        init(); //����ǳ�ʼ���µ�����,���ܶ���Ϊ0
        return true;
    }

    while(1)
    {
        //���ʣ������ǰsockfd��ɾ���ˣ������������ֶδ����𣿿��ܻᣬ����õ���(����ÿ���Ӧʱ�䣬��write��ʱ�䣬�Լ����߳��ж�ʱ������ǰm_sockfd�ĳ�ʱʱ�䣩
        //һ��epoll��������ô�ͻ���ֺܶ����
        //���忴��ȸ�������ص㣩��Ŀȱ�㣺����epoll�����������ʱ�����ֵ����⡿
        temp=m_client->writev(m_iv,m_iv_count);
        if(temp<0) //��ʾд��ʧ��
        {
            if (errno == EAGAIN) //���ֿ��ܻ�û��д���Ȩ������ô���Ǿͽ����עд������������ǰЭ�̸���ȥ,Ȼ�󵽵��ʱ�����
            {
                return true;
            }
            unmap();
            return false;
        }

        bytes_have_send += temp; //д��ĸ���+
        bytes_to_send -= temp;   //��д�ĸ���-
        if(bytes_have_send>=m_iv[0].iov_len) //˵����һ�������ݷ������ˣ�Ȼ��ʼ�Եڶ������в���
        {
            m_iv[0].iov_len=0;
            m_iv[1].iov_base=m_file_address+(bytes_have_send-m_write_idx);
            m_iv[1].iov_len=bytes_to_send;
        }
        else 
        {
            m_iv[0].iov_base=m_write_buf+bytes_have_send;
            m_iv[0].iov_len=m_iv[0].iov_len-bytes_have_send;

        }
        //bytes_to_send<=0��ʱ�Ѿ��ɹ�д�뵽�ͻ�����,�����ݷ��͹�ȥ��
        if(bytes_to_send<=0)
        {
            unmap();
            //Ϊ�˱�֤�Ƕ����ӣ���Ȼ��ʱ���ý�������ɶ���������handle���Զ�ɾ������¼����Լ�������
            //�����������Ϊ�˲鿴������������Ƿ��ǳ������ӵģ�����ǣ���ô��true������Ϊfalse,Ȼ���Ӧ��ʱ����Щ�ᱻ�̳߳ظ������
            if(m_linger)
            {
                init();
                return true;
            }
            else 
                return false;

        }

    }

}

//���������Ӧ���֣�����������������־�ϵģ�ͨ�������������־��ͨ���׽��ֺ����������write��
bool http_conn::add_response(const char * format,...)
{
    if(m_write_idx>=WRITE_BUFFER_SIZE)
        return false;   
    va_list arg_list;
    va_start(arg_list,format);   //����ǿɱ��������ɶ�Ŀ�ͷ�����Ա�֤�ɱ��������һЩδ�������Ϊ
    int len=vsnprintf(m_write_buf+m_write_idx,WRITE_BUFFER_SIZE-1-m_write_idx,format,arg_list); //֧�ֿɱ��������һЩ����ֱ�ӷ��뵽m_write_buf��
    //m_write_idxΪƫ������С
    if (len >= (WRITE_BUFFER_SIZE - 1 - m_write_idx))
    {
        va_end(arg_list);
        return false;
    }

    m_write_idx+=len;
    va_end(arg_list);
    //����Ӧ���ݴ�һ����־�У����������Ŀ��
    if(Config::get_instance()->get_close_log()==0) {
        LOG_INFO << "request: " << m_write_buf;
    }
    return true;
}

bool http_conn::add_status_line(int status,const char * title)
{
    return add_response("%s %d %s\r\n","HTTP/1.1",status,title);
}

bool http_conn::add_headers(int content_len)
{
    return add_content_length(content_len)&&add_linger()&&add_blank_line();
}

bool http_conn::add_content_length(int content_len)     //�����ݳ��ȸ�Ū���������䳤���������־��
{
    return add_response("Content-Length:%d\r\n",content_len);
}


bool http_conn::add_content_type()
{
    return add_response("Content-Type:%s\r\n","text/html");
}

bool http_conn::add_linger()
{
    return add_response("Connection:%s\r\n", (m_linger == true) ? "keep-alive" : "close"); //�����Ƿ�����״̬��������Ӧ����־��
}

bool http_conn::add_blank_line() //���ڻ��е�
{
    return add_response("%s", "\r\n");
}

bool http_conn::add_content(const char * content)
{
    return add_response("%s",content);
}

//�������������ǽ���Ҫ���͵���Ӧ����׼���ã������ú� iovec �ṹ��m_iv�������ֽ��� (bytes_to_send)���Ա����ͨ�� write ����ʵ�ʷ��͡�
//��Ҫ���𹹽� HTTP ��Ӧ���ݣ���׼�������ݽṹ������������ʹ��
bool http_conn::process_write(HTTP_CODE ret)
{
    switch(ret)
    {
        case INTERNAL_ERROR:
        {
            add_status_line(500,error_500_title);
            add_headers(strlen(error_500_form) );
            if(!add_content(error_500_form) )
                return false;
            break;
        }
        case BAD_REQUEST:
        {
            add_status_line(404, error_404_title);
            add_headers(strlen(error_404_form));
            if (!add_content(error_404_form))
                return false;
            break;
        }
        case FORBIDDEN_REQUEST:
        {
            add_status_line(403, error_403_title);
            add_headers(strlen(error_403_form));
            if (!add_content(error_403_form))
                return false;
            break;
        }
        case FILE_REQUEST:
        {
            add_status_line(200,ok_200_title);
            if(m_file_stat.st_size!=0) //���ȼ���ļ���С�Ƿ�Ϊ0
            {
                add_headers(m_file_stat.st_size);
                m_iv[0].iov_base=m_write_buf;
                m_iv[0].iov_len=m_write_idx;
                m_iv[1].iov_base=m_file_address;
                m_iv[1].iov_len=m_file_stat.st_size;
                m_iv_count=2; //��ʾҪ�����������ݿ�
                bytes_to_send=m_write_idx+m_file_stat.st_size;
                return true;
            }
            else 
            {
                const char *ok_string="<html><body></body></html>";
                add_headers(strlen(ok_string));
                if(!add_content(ok_string) ){
                    return false;
                }
            }
        }

        default:
            return false;
    }
    m_iv[0].iov_base = m_write_buf;
    m_iv[0].iov_len = m_write_idx;
    m_iv_count = 1;
    bytes_to_send = m_write_idx;
    return true;

}

bool http_conn::process()
{
    HTTP_CODE read_ret;
    if( (read_ret=process_read() )==NO_REQUEST)       //
    {
        return false;
    }
    bool write_ret = process_write(read_ret);
    if (!write_ret)
    {
        close_conn();
        return false;
    }
    return true;
}











