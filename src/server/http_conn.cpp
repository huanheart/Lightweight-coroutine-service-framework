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
//替换为更权威的LRU数据结构
LRUCache<std::string,std::string,MemoryPool<std::string> > users;

//（注意，这个不能放入到http_conn.h中）因为每个用户对应了很多个http类，而不是全局的users来进行管理
//但是放入到这里就可以做到全局的一个管理了(即所有用户的数据都存储到这个地方)

AbstractFactory * factory=nullptr;    //这个处理的时候可能需要加锁
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
    //从连接池里面获取一个连接
    MYSQL* mysql=nullptr;
    connectionRAII mysqlcon(&mysql,conn_pool);
    //在user表中检索username,passwd数据，看是否可以满足不报错的条件
    if(mysql_query(mysql,"select username,passwd from user") ) //如果返回非0，说明内部错误
    {
        if(Config::get_instance()->get_close_log()==0) {
            LOG_INFO << "select error: " << mysql_error(mysql);
        }
    }

    //从表中检索完整的结果集：
    MYSQL_RES *result=mysql_store_result(mysql);

    //返回结果集的列数
    int num_fields=mysql_num_fields(result);

    //返回所有字段结构的数组
    MYSQL_FIELD * fields=mysql_fetch_fields(result); //这两个有什么用吗？定义局部变量也没看到有啥利用场景

    while(MYSQL_ROW row=mysql_fetch_row(result) )
    {
        std::string temp1(row[0]);
        std::string temp2(row[1]);
        users.push(temp1,temp2);  //存放到对应的内存中，其实可以弄一个redis，存放到里面
    }
}

void http_conn::initproxy(int decideproxy)
{
    //使用工厂模式，创建对应工厂以及代理类
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



int http_conn::m_user_count=0; //静态成员变量初始化

//关闭连接，客户总量-1
//可以发现，此时关闭了连接，但是似乎没有关闭当前定时器！
//待发现是否存在bug
void http_conn::close_conn(bool real_close)
{
    if(real_close&&(m_client->getSocket()!=-1) )
    {
        printf("close %d\n",m_client->getSocket());
        m_user_count--;
    }

}


//初始化连接，外部调用初始化套接字地址
void http_conn::init(sylar::Socket::ptr &client,char * root,int TRIGMode,
                     std::string user,std::string passwd,std::string sqlname,bool keepalive)
{
    this->m_client=client;
    m_user_count++;
    m_linger=keepalive;
 //当浏览器出现连接重置时，可能是网站根目录出错或http响应格式出错或者访问的文件中内容完全为空 //这里不懂
    doc_root=root; //这个应该是服务器的根目录
    //(得加到config里面）
    m_TRIGMode=TRIGMode; //
    // (得加到config里面）
    strcpy(sql_user, user.c_str());
    strcpy(sql_passwd, passwd.c_str());
    strcpy(sql_name, sqlname.c_str());
    init(); //内部重置初始化
}

//初始化新接收的连接
//check_state默认为分析请求行状态
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


//从状态机获取状态，用于分析出一行内容
//返回值为行的读取状态，有LINE_OK,LINE_BAD,LINE_OPEN
http_conn::LINE_STATUS http_conn::parse_line() //用于解析不同的状态（解析每一行）
{
    char temp;
    for (; m_checked_idx < m_read_idx; ++m_checked_idx)         //m_read_idx是一开始用水平触发或者边缘触发读取的内容数据的长度，数据是存放到m_read_buf中的，通过webserver调用read_once
    {
        temp = m_read_buf[m_checked_idx];
        if(temp=='\r') //回车符号
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
            if(m_checked_idx>1&&m_read_buf[m_checked_idx-1] =='\r')  //-1和+1的目的是防止溢出
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

//循环读取客户数据，直到无数据可读或对方关闭连接
//非阻塞ET工作模式下，需要一次性将数据读完
//process_read 依赖 read_once 提供的数据：read_once 将数据从 socket 中读取到缓冲区，然后 process_read 在缓冲区中解析数据。
//read_once 只负责数据读取，而 process_read 负责数据解析,后者可以真正做到数据读取的拦截
bool http_conn::read_once()
{
    if(m_read_idx>=READ_BUFFER_SIZE)
        return false;
    int bytes_read = 0;

    //LT读取数据(水平触发)
    if(m_TRIGMode==0)
    {
        bytes_read=m_client->recv(m_read_buf+m_read_idx,READ_BUFFER_SIZE-m_read_idx,0);
        m_read_idx+=bytes_read;

        if(bytes_read<=0)
            return false;
        return true;
    }
    //ET读取数据
    else{
        //由于这个是需要一次性读取完数据的，固然有循环et模式和非循环et模式，
        while(true)
        {
            bytes_read=m_client->recv(m_read_buf + m_read_idx, READ_BUFFER_SIZE - m_read_idx, 0);
            if(bytes_read==-1)
            {
                if(errno==EAGAIN || errno==EWOULDBLOCK) //说明后面没有数据了，固然退出循环
                    break;
                return false;  //return false说明内部出错了
            }
            else if(bytes_read==0) //说明根本就没有读的内容
            {
                return false;
            }
            m_read_idx+=bytes_read;
           
        }   
        return true;
    }

}

//解析http请求行，获得请求方法，目标url以及版本号
//这里解析的内容主要放置到了类的成员先储存起来了，然后后面会用到
//请求行的意思详情看语雀笔记
http_conn::HTTP_CODE    http_conn::parse_request_line(char* text)
{
    m_url=strpbrk(text," \t"); //返回出现在这个第二个字符串集合中的第一个属于字符串1的下标的后面的内容
    if(!m_url)
    {   
        return BAD_REQUEST;
    }
    *m_url++='\0';
    char * method=text;

    if(strcasecmp(method,"GET")==0) //用于比较的函数
        m_method=GET;
    else if(strcasecmp(method,"POST")==0)
    {
        m_method=POST;
        cgi=1; //这个cgi是用来干嘛的？是否启用的post
    }
    else {
     
        return BAD_REQUEST; ///如果都没有这两个请求，这个项目只处理了get和post
    }

    m_url+=strspn(m_url," \t" );  //具体看语雀
    


    m_version=strpbrk(m_url," \t");
    if (!m_version){
        
        return BAD_REQUEST;
    }
    *m_version++ = '\0';
    
    m_version += strspn(m_version, " \t");

    if (strcasecmp(m_version, "HTTP/1.1") != 0) //如果不相等
    {
   
        return HTTP_CODE::BAD_REQUEST;
    }

    if(strncasecmp(m_url,"http://",7)==0) //判断两个字符串的前7个字符是否相等
    {
        m_url+=7;
        m_url=strchr(m_url,'/'); //可能这个m_url的http://这里已经没用了，固然往后了，这样比较好后面的判断
    }


    if (strncasecmp(m_url, "https://", 8) == 0)
    {
        m_url += 8;
        m_url = strchr(m_url, '/');
    }

    if(!m_url || m_url[0]!='/' ) //说明出现了错误了，可能后面没有内容了，所以没有截取到'/'
    {
        return BAD_REQUEST;
    }
    //当url为/时，显示判断界面         为什么此时要
    if(strlen(m_url)==1)
        strcat(m_url,"judge.html");
    m_check_state= CHECK_STATE_HEADER;
    return NO_REQUEST;
}

//解析http请求的一个头部信息(逐行解析)
//这里的内容会被循环调用
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
        ////已删除根据客户端的意愿而设置是否为长还是短连接(改成服务端自行选择）
//        if(strcasecmp(text,"keep-alive")==0)
//        {
//            m_linger=true;
//        }
    }
    else if (strncasecmp(text, "Content-length:", 15) == 0)
    {
        text+=15;
        text+=strspn(text," \t");
        m_content_length=atol(text); //获取这个字符串的长度
    }
    else if(strncasecmp(text,"Host:",5)==0 )   //比较text字符串前5个字符组成的字符串是否与Host:匹配
    {
        text+=5;
        text+=strspn(text," \t"); //重置这里为开头
        m_host=text;             //获取其内容
    }
    else if(strncasecmp(text,"X-Forwarded-By:",15)==0 ) //说明是nginx转发的
    {
        if(obj->ProxyType==1) //如果没有选择nginx反向代理，固然将nginx访问过来的数据进行拦截
        {
            return BAD_REQUEST;
        }
        decide_proxy=false; //为了最后进行判断的时候用到的（判断有nginx的时候是否用了对应的无nginx模式）
        
    }
    else
    {
        //输出到日志系统，查看头部有什么额外的标识，可能是发送方自己多额外增加了一些标识
        if(Config::get_instance()->get_close_log()==0) {
            LOG_INFO << "oop!unknow header: " << text;
        }
    }
    
    return NO_REQUEST;
}


//判断http请求是否被完整读入
http_conn::HTTP_CODE http_conn::parse_content(char * text)
{
    if(m_read_idx>=(m_content_length+m_checked_idx) )
    {
        text[m_content_length]='\0';
        //post请求中最后为输入的用户名和密码
        m_string=text;
        return GET_REQUEST;
    }
    return NO_REQUEST;
}


//process_read 是处理 HTTP 协议的核心逻辑，解析请求并判断如何响应
//process_read 依赖 read_once 提供的数据
http_conn::HTTP_CODE  http_conn::process_read()
{
    LINE_STATUS line_status=LINE_OK;
    HTTP_CODE ret=NO_REQUEST;
    char * text=0;
    //判断请求的内容
    bool decide_proxy=true; 
    //默认是没用nginx(这个变量一定是需要局部变量的，而不是全局变量)
    //否则出现了全局的错误（即有一个nginx的请求过来，那么此时将变量设置了false，那么再来一个9006端口，就会误认为是nginx请求过来的）

    //这个循环一开始默认是进入 ||右边的那个 ( (line_status=parse_line() )==LINE_OK，后面弄完之后就会开始请求头以及请求体的处理了
    while( (m_check_state==CHECK_STATE_CONTENT &&line_status==LINE_OK)
          || ( (line_status=parse_line() )==LINE_OK) ) //分析当前行状态是否和此时设置的line_status状态一致
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
                    return BAD_REQUEST;       //比如说不能处理put这些请求的情况，会出现BAD_REQUEST
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
                if(ret==GET_REQUEST) //说明是一个完整的GET请求
                    return do_request(decide_proxy);
                line_status=LINE_OPEN;
                break;
            }
            default:
                return INTERNAL_ERROR; //服务器内部错误

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

    if(decide_proxy==false){ //表示了用nginx，那么查看obj是否为1(1代表没有用)
        if(obj->ProxyType==1) //固然矛盾
            return BAD_REQUEST;
    }else {
        if(obj->ProxyType==0){
            return BAD_REQUEST;
        }
    }

    strcpy(m_real_file,doc_root); //将文件的根目录，然后赋值到m_real_file这里
    int len=strlen(doc_root);
    const char * p=strrchr(m_url,'/'); //里面的值不能变,查找'/'第一次出现的位置

    //处理cgi，判断是哪种请求方式
    if(cgi==1&&(*(p+1)=='2' || *(p+1)=='3' ) )
    {
        //根据标志判断是登录检测还是注册检测
        char flag=m_url[1];
        char *m_url_real=(char *)malloc(sizeof(char) *200);
        strcpy(m_url_real, "/");
        strcat(m_url_real,m_url+2); //将m_url+2后面的字符串添加到m_url_real中
        strncpy(m_real_file+len,m_url_real,FILENAME_LEN-len-1); 
        //将m_url_real的字符串复制前第三个参数然后给到第一个字符串中
        free(m_url_real); //释放操作
    
      //将用户名和密码提取出来
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
        //如果是注册，先检测数据库中是否有重名的
        //没有重名才允许加
        char * sql_insert=(char*)malloc(sizeof(char)*200);
        strcpy(sql_insert, "INSERT INTO user(username, passwd) VALUES(");
        strcat(sql_insert, "'");
        strcat(sql_insert, name);
        strcat(sql_insert, "', '");
        strcat(sql_insert, password);
        strcat(sql_insert, "')");
        //上面这一个主要是拼接一个sql语句而已
//        BackNode<std::string> back;
//        back=users.find(name);
        pair<bool,std::string> back=users.get(name);
        if(back.first==false) //说明没有找到,用户表维护在一个map中
        {
            m_lock.lock();
	    // std::cout<<"test mysql address "<<mysql<<std::endl;
            int res=mysql_query(mysql,sql_insert); //访问连接的数据库里面是否有记录
	
	    users.push(name,password);
            m_lock.unlock();
            if(!res) //说明没有，那么直接让他弄到登录界面，先进行一个赋值
                strcpy(m_url,"/log.html");
            else 
                strcpy(m_url, "/registerError.html");
        }
        else 
            strcpy(m_url, "/registerError.html");
        //如果是登录，直接判断
        //若浏览器端输入的用户名和密码在表中可以查找到，返回1，否则返回0
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
//上面的2或者3代表登录以及注册
//下面的就是其他状态的界面了

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
    ////stat 函数将把所获取的文件状态信息填充到这个结构体中文件类型（如常规文件、目录等）
    ////文件权限
    ////文件大小
    ////最近访问时间、修改时间、创建时间等
    if(stat(m_real_file,&m_file_stat)<0)  //从里面获取文件当前的信息,stat()函数获取对应位置的文件信息
    {
        return NO_RESOURCE; //没有资源
    }
    ////如果不可读，则返回
    if(!(m_file_stat.st_mode)&S_IROTH) {
        return FORBIDDEN_REQUEST; //禁止请求的状态码
    }
    ////判断当前文件路径是不是目录
    if(S_ISDIR(m_file_stat.st_mode) )
        return BAD_REQUEST;

    int fd=open(m_real_file,O_RDONLY); //只读的方式打开这个文件，然后由于在linux下，返回一个文件描述符（这个文件就是要发送的文件了）
        ////  MAP_PRIVATE 对映射区域的写入操作会产生一个映射文件的复制，即私人的“写入时复制”（copy on write）对此区域作的任何修改都不会写回原来的文件内容
        /// ////m_file_address为系统帮你映射的那个内存地址
    m_file_address=(char*)mmap(0,m_file_stat.st_size,PROT_READ,MAP_PRIVATE,fd,0); //做映射，具体看语雀(这个和要传输的视频文件相关联的)(它也可以做到内存共享)
    //这里主要是将一个普通文件映射到内存中，通常在需要对文件进行频繁读写时使用
    ::close(fd);
    
    return FILE_REQUEST;

}

//解除上面函数最后所做的那个映射
void http_conn::unmap()
{
    if(m_file_address)
    {
        munmap(m_file_address,m_file_stat.st_size);
        m_file_address=0;
    }

}


//write 则负责实际将数据发送给客户端，处理发送中的错误、进度更新等操作。
bool http_conn::write()
{
    int temp=0;
    if(bytes_to_send==0) //发送字节数==0
    {
        //改成关注读事件
        init(); //这个是初始化新的连接,将很多置为0
        return true;
    }

    while(1)
    {
        //疑问：如果当前sockfd被删除了，这个函数会出现段错误吗？可能会，具体得调试(具体得看响应时间，即write的时间，以及主线程中定时器处理当前m_sockfd的超时时间）
        //一旦epoll数量大，那么就会出现很多错误
        //具体看语雀：【（重点）项目缺点：考虑epoll监听数量大的时候会出现的问题】
        temp=m_client->writev(m_iv,m_iv_count);
        if(temp<0) //表示写入失败
        {
            if (errno == EAGAIN) //这种可能还没有写入的权力，那么我们就将其关注写的能力，将当前协程给出去,然后到点的时候回来
            {
                return true;
            }
            unmap();
            return false;
        }

        bytes_have_send += temp; //写入的个数+
        bytes_to_send -= temp;   //待写的个数-
        if(bytes_have_send>=m_iv[0].iov_len) //说明第一个的数据发送完了，然后开始对第二个进行操作
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
        //bytes_to_send<=0此时已经成功写入到客户端了,将数据发送过去了
        if(bytes_to_send<=0)
        {
            unmap();
            //为了保证是短连接，固然此时不用将其监听成读，后面在handle中自动删除这个事件，以及描述符
            //这个的作用是为了查看这个外来请求是否是长短连接的，如果是，那么就true，否则为false,然后对应定时器这些会被线程池给处理掉
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

//这个不是响应部分，这个是用于输出到日志上的，通过流，输出到日志是通过套接字函数来输出的write吧
bool http_conn::add_response(const char * format,...)
{
    if(m_write_idx>=WRITE_BUFFER_SIZE)
        return false;   
    va_list arg_list;
    va_start(arg_list,format);   //这个是可变参数的那啥的开头，可以保证可变参数产生一些未定义的行为
    int len=vsnprintf(m_write_buf+m_write_idx,WRITE_BUFFER_SIZE-1-m_write_idx,format,arg_list); //支持可变参数，将一些内容直接放入到m_write_buf中
    //m_write_idx为偏移量大小
    if (len >= (WRITE_BUFFER_SIZE - 1 - m_write_idx))
    {
        va_end(arg_list);
        return false;
    }

    m_write_idx+=len;
    va_end(arg_list);
    //将对应内容打到一个日志中，这个是最终目的
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

bool http_conn::add_content_length(int content_len)     //将数据长度给弄出来，将其长度输出到日志上
{
    return add_response("Content-Length:%d\r\n",content_len);
}


bool http_conn::add_content_type()
{
    return add_response("Content-Type:%s\r\n","text/html");
}

bool http_conn::add_linger()
{
    return add_response("Connection:%s\r\n", (m_linger == true) ? "keep-alive" : "close"); //将其是否连接状态发给到对应的日志上
}

bool http_conn::add_blank_line() //用于换行的
{
    return add_response("%s", "\r\n");
}

bool http_conn::add_content(const char * content)
{
    return add_response("%s",content);
}

//这个函数的输出是将需要发送的响应数据准备好，并设置好 iovec 结构（m_iv）和总字节数 (bytes_to_send)，以便后续通过 write 函数实际发送。
//主要负责构建 HTTP 响应数据，并准备好数据结构，供后续发送使用
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
            if(m_file_stat.st_size!=0) //首先检查文件大小是否为0
            {
                add_headers(m_file_stat.st_size);
                m_iv[0].iov_base=m_write_buf;
                m_iv[0].iov_len=m_write_idx;
                m_iv[1].iov_base=m_file_address;
                m_iv[1].iov_len=m_file_stat.st_size;
                m_iv_count=2; //表示要发送两个数据块
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











