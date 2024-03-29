#ifndef HTTP_CONN_H
#define HTTP_CONN_H

#include <sys/epoll.h>
#include <cstdio>
#include <cstdlib>
#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <stdarg.h>
#include <errno.h>
#include "locker.h"
#include <sys/uio.h>
#include <string.h>

class http_conn {
public:
    
    static int m_epollfd;                       //所有的socket上事件都被注册在同一个epoll上
    static int m_user_count;                    //统计用户数量
    static const int READ_BUFFER_SIZE = 2048;   //读缓冲区大小
    static const int WRITE_BUFFER_SIZE = 2048;  //写缓冲区大小
    static const int FILENAME_LEN = 200;         //请求的文件名最大长度
    // HTTP请求的方法，该项目只支持get
    enum METHOD {
        GET = 0,
        POST,
        HEAD,
        PUT,
        DELETE,
        TRACE,
        OPTIONS,
        CONNECT
    };
    /* 
        解析客户端请求时，主状态机的状态
        CHECK_STATE_REQUESTLINE:正在分析请求行
        CHECK_STATE_HEADER:正在分析头部字段
        CHECK_STATE_CONTENT:正在解析请求体
    */
    enum CHECK_STATE {
        CHECK_STATE_REQUESTLINE = 0,
        CHECK_STATE_HEADER,
        CHECK_STATE_CONTENT
    };
    /*
        从状态机的三种可能状态，即行的读取状态
        LINE_OK:读取到一个完整的行
        LINE_BAD:行出错
        LINE_OPEN:行数据尚且不完整
    */
    enum LINE_STATUS {
        LINE_OK = 0,
        LINE_BAD,
        LINE_OPEN
    };
    /*
        服务器处理HTTP请求的可能结果，报文解析的结果
        NO_REQUEST          :请求不完整,需要继续读取客户端数据
        GET_REQUEST         :表示获得了一个完成的客户请求
        BAD_REQUEST         :表示客户请求语法错误
        NO_RESOURCE         :表示服务器没有资源
        FORBIDDEN_REQUEST   :表示客户对资源没有足够的访问权限
        FILE_REQUEST        :文件请求,获取文件成功
        INTERNAL_ERROR      :表示服务器内部错误
        CLOSED_CONNECTION   :表示客户端已经关闭连接了
    */
    enum HTTP_CODE {
        NO_REQUEST,
        GET_REQUEST,
        BAD_REQUEST,
        NO_RESOURCE,
        FORBIDDEN_REQUEST,
        FILE_REQUEST,
        INTERNAL_ERROR,
        CLOSED_CONNECTION
    };
    http_conn() {}
    ~http_conn() {}
    void process();                                 //处理客户端请求，由线程池中的工作线程调用，是处理HTTP请求的入口函数
    void init(int sockfd, const sockaddr_in &addr); //初始化连接
    void close_conn();                              //关闭连接
    bool read();                                    //一次性把所有数据都读完，非阻塞的读
    bool write();                                   //一次性写完所有数据，非阻塞的写


private:
    const char *doc_root = "/what-can-i-do-/resources"; //文件根目录
    char m_real_file[FILENAME_LEN];     //请求的文件名称
    struct stat m_file_stat;            //请求的文件状态
    char *m_file_address;               //映射的文件地址
    struct iovec m_iv[2];
    int m_iv_count;
    int bytes_to_send;                  //将要发送的数据字节数
    int bytes_have_send;                //已经发送的字节数

    int m_sockfd;                       // 该http连接的socket
    sockaddr_in m_address;              //通信的socket地址
    char m_read_buf[READ_BUFFER_SIZE];  //读缓冲区
    int m_read_idx;                     //标识读缓冲区中已经读入的客户端的最后一个字节的下一个位置
    char m_write_buf[ WRITE_BUFFER_SIZE ];
    int m_write_idx;                    //写缓冲区中待发送的字节数。
    int m_checked_idx;                  //当前正在分析的字符在读缓冲区的位置
    int m_start_line;                   //当前正在解析的行的起始位置
    int m_content_length;               //消息体的长度
    CHECK_STATE m_check_state;          // 主状态机当前所处的状态
    char *m_url;                        //请求目标文件的文件名
    char *m_version;                    //HTTP协议版本，只支持HTTP1.1
    METHOD m_method;                    //请求方法
    char *m_host;                       //主机名
    bool m_linger;                      //HTTP请求是否要保持连接keep-alive

    void init();                                //初始化其他的信息
    HTTP_CODE process_read();                   //解析HTTP请求
    bool process_write(HTTP_CODE ret);          //根据解析的HTTP请求，返回给客户端内容
    HTTP_CODE parse_request_line(char *text);   // 解析请求行
    HTTP_CODE parse_headers(char *text);        //解析请求头
    HTTP_CODE parse_content(char *text);        //解析请求体
    LINE_STATUS parse_line();                   //解析一行是否完整
    HTTP_CODE do_request();                     //处理请求
    void unmap();                               //对内存映射区执行munmap操作
    char *get_line() { return m_read_buf + m_start_line; }
    bool add_response(const char *format, ...);             //添加响应
    bool add_status_line(int status, const char *title);    //添加响应状态行
    bool add_content( const char* content );                //添加响应内容
    bool add_content_type();                                //添加响应内容类型
    bool add_headers(int content_length);                   // 添加http响应报文的头部字段
    bool add_content_length( int content_length );          //添加内容实体的长度
    bool add_linger();                                      //添加响应报文的Connection字段，指定是否保持连接
    bool add_blank_line();                                  //添加\r\n
};

#endif