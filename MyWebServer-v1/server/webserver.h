#ifndef WEBSERVER_H
#define WEBSERVER_H
#include <unordered_map>
#include <fcntl.h>
#include <unistd.h>
#include <assert.h>
#include <errno.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <memory>

#include "epoller.h"
#include "../log/log.h"
#include "../timer/heaptimer.h"
#include "../http/httpconn.h"
#include "../sqlpool/sqlpool.h"
#include "../sqlpool/sqlpoolRAII.h"
#include "../threadpool/threadpool.h"

class WebServer {
public:
    WebServer(
        int port, int trigMode, int timeoutMS, bool OptLinger, 
        int sqlPort, const char* sqlUser, const  char* sqlPwd, 
        const char* dbName, int connPoolNum, int threadNum,
        bool openLog, int logLevel, int logQueSize);
    ~WebServer();
    void Start();

private:
    bool InitSocket_();
    void InitEventMode_(int trigMode);
    void AddClient(int fd, sockaddr_in addr);

    void DealListen_();
    void DealWrite_(HttpConn* client);
    void DealRead_(HttpConn* client);

    void SendError_(int fd, const char* info);
    void ExtentTime_(HttpConn* client);
    void CloseConn_(int fd);

    void OnRead_(HttpConn* client);
    void OnWrite_(HttpConn* client);
    void OnProcess_(HttpConn* client);

private:
    static const int MAX_FD = 65536; // 最大连接数
    static int SetFdNonblock(int fd); // 设置非阻塞
    int port_;        // 端口
    bool openLinger_; // 是否延迟关闭
    int timeoutMS_;   // epoll_wait阻塞时间
    bool isClose_;    // 服务是否关闭
    int listenFd_;    // 监听fd
    char* srcDir_;    // 资源跟路径

    uint32_t listenEvent_; // 监听事件属性
    uint32_t connEvent_;   // 

    std::unique_ptr<HeapTimer> timer_;       // 定时器
    std::unique_ptr<ThreadPool> threadpool_; // 线程池
    std::unique_ptr<Epoller> epoller_;       // epoll工具
    std::unordered_map<int, HttpConn> users_;// 用户client
};

#endif