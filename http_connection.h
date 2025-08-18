#ifndef HTTP_CONNECTION_H
#define HTTP_CONNECTION_H

#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/epoll.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <assert.h>
#include <sys/stat.h>
#include <string.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <stdarg.h>
#include <errno.h>
#include <sys/wait.h>
#include <sys/uio.h>
#include <map>
#include "locker.h"
#include "sql_connection_pool.h"
#include "list_timer.h"
#include "log.h"

using std::map;
using std::pair;

class http_connection {
public:
    static const int FILENAME_LEN = 200;       // 文件名长度
    static const int READ_BUFFER_SIZE = 2048;  // 读buf长度
    static const int WRITE_BUFFER_SIZE = 1024; // 写buf长度
    // 请求类型
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
    // 解析状态
    enum CHECK_STATE
    {
        CHECK_STATE_REQUESTLINE = 0, // 解析请求行
        CHECK_STATE_HEADER,        // 解析请求头
        CHECK_STATE_CONTENT        // 解析请求体
    };
    // 解析类型
    enum HTTP_CODE
    {
        NO_REQUEST, // 未解析完
        GET_REQUEST,
        BAD_REQUEST,
        NO_RESOURCE, // 没有要访问的资源
        FORBIDDEN_REQUEST,  // 进制访问请求
        FILE_REQUEST,     // 文件请求
        INTERNAL_ERROR,   // 内部错误
        CLOSED_CONNECTION // 关闭连接
    };
    // 解析一行
    enum LINE_STATUS
    {
        LINE_OK = 0,      // 解析成功
        LINE_BAD,         // 格式错误
        LINE_OPEN         // 还未读完
    };

public:
    http_connection() {}
    ~http_connection() {}

public: // 公共接口
    void init(int sockfd, const sockaddr_in &addr, char *, int, int, string user, string passwd, string sqlname);
    void close_conn(bool real_close = true);
    void process();
    bool read_once();
    bool write();
    sockaddr_in *get_address()
    {
        return &_address;
    }
    void initmysql_result(sql_connection_pool *connPool);

private: // 私有方法
    void init();
    HTTP_CODE process_read();
    bool process_write(HTTP_CODE ret);
    HTTP_CODE parse_request_line(char *text);
    HTTP_CODE parse_headers(char *text);
    HTTP_CODE parse_content(char *text);
    HTTP_CODE do_request();
    char *get_line() { return _read_buf + _start_line; };
    LINE_STATUS parse_line();
    void unmap();
    bool add_response(const char *format, ...);
    bool add_content(const char *content);
    bool add_status_line(int status, const char *title);
    bool add_headers(int content_length);
    bool add_content_type();
    bool add_content_length(int content_length);
    bool add_linger();
    bool add_blank_line();

public: 
    static int _epoll_fd; // 全局epoll_fd
    static int _user_count; // 全局在线用户数量
    MYSQL* mysql;         // 当前http连接使用的mysql连接
    int state;          // 读为0， 写为1
    int timer_flag;
    int improv;

private: // 私有属性
    int _sockfd;                        // 客户端socket_fd
    struct sockaddr_in _address;               // 客户端地址
    char _read_buf[READ_BUFFER_SIZE];   // 读缓冲池
    int _read_idx;                      // 读缓冲池数据索引下标
    int _checked_idx;                   // 解析器当前位置
    int _start_line;                    // 当前行起始位置
    char _write_buf[WRITE_BUFFER_SIZE]; // 写缓冲池
    int _write_idx;                     // 写缓冲池索引
    CHECK_STATE _check_state;
    METHOD _method;
    char _real_file[FILENAME_LEN];      // 目标文件绝对路径
    char *_url;          // m_read_buf中的url
    char *_version;      // m_read_buf中的 HTTP1.1
    char *_host;         // m_read_buf中的 主机名
    int _content_length; // 请求体长度
    bool _linger;        // 是否为长连接
    char *_file_address; // mmap映射后的文件地址
    struct stat _file_stat; // 文件元信息
    struct iovec _iv[2];    // readv writev 读写向量
    int _iv_count;          // readv writev 参数
    int cgi;        //是否启用的POST
    char *_string;  //存储请求体数据
    int bytes_to_send;    // 要发送的数据量
    int bytes_have_send;  // 已经发送的数据量

    char *doc_root;
    map<string, string> _users;
    int _trigMode;
    int _close_log;

    char _sql_user[100];
    char _sql_passwd[100];
    char _sql_name[100];
};



#endif