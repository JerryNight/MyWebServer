#include "webserver.h"



WebServer::WebServer(
    int port, int trigMode, int timeoutMS, bool OptLinger, 
    int sqlPort, const char* sqlUser, const  char* sqlPwd, 
    const char* dbName, int connPoolNum, int threadNum,
    bool openLog, int logLevel, int logQueSize)
    {
        // 初始化
        port_ = port;
        openLinger_ = OptLinger;
        timeoutMS_ = timeoutMS;
        isClose_ = false;
        timer_ = std::make_unique<HeapTimer>();
        threadpool_ = std::make_unique<ThreadPool>();
        epoller_ = std::make_unique<Epoller>();

        // 资源根路径
        srcDir_ = getcwd(nullptr, 256);
        assert(srcDir_);
        strncat(srcDir_, "/resources", 16);
        // 初始化httpcon的静态成员
        HttpConn::userCount = 0;
        HttpConn::srcDir = srcDir_;
        // 初始化数据库连接池
        SqlPool::GetInstance()->init("127.0.0.1",sqlPort,sqlUser,sqlPwd,dbName,connPoolNum);

        InitEventMode_(trigMode); // 设置触发模式
        if (!InitSocket_()) isClose_ = true;

        if (openLog) {
            Log::GetInstance()->init(logLevel, "./log",".log",logQueSize);
            if (isClose_) {
                LOG_ERROR("======== Server int error! ========");
            } else {
                LOG_INFO("========== Server init ==========");
                LOG_INFO("Port:%d, OpenLinger: %s", port_, OptLinger? "true":"false");
                LOG_INFO("Listen Mode: %s, OpenConn Mode: %s",
                                (listenEvent_ & EPOLLET ? "ET": "LT"),
                                (connEvent_ & EPOLLET ? "ET": "LT"));
                LOG_INFO("LogSys level: %d", logLevel);
                LOG_INFO("srcDir: %s", HttpConn::srcDir);
                LOG_INFO("SqlConnPool num: %d, ThreadPool num: %d", connPoolNum, threadNum);
            }
        }
    }
WebServer::~WebServer(){
    close(listenFd_);
    isClose_ = true;
    free(srcDir_);
    SqlPool::GetInstance()->ClosePool();
}

/**
 * 
 */
void WebServer::Start(){
    int timeMS = -1; // epoll_wait() 不阻塞
    if (!isClose_) LOG_INFO("========== Server start ==========");
    while (!isClose_) {
        if (timeoutMS_ > 0) {
            timeMS = timer_->GetNextTick();
        }
        // epoll 监听
        int eventCnt = epoller_->Wait(timeMS);
        for (int i = 0; i < eventCnt; i++){
            // 监听到事件
            int fd = epoller_->GetEventFd(i);
            uint32_t events = epoller_->GetEvents(i);
            if (fd == listenFd_) {
                DealListen_();
            } else if (events & (EPOLLRDHUP | EPOLLHUP | EPOLLERR)){
                CloseConn_(&users_[fd]);
            } else if (events & EPOLLIN) {
                DealRead_(&users_[fd]);
            } else if (events & EPOLLOUT) {
                DealWrite_(&users_[fd]);
            } else {
                LOG_ERROR("Unexpected event!");
            }
        }
    }
}

/**
 * InitSocket() 网络编程
 * 1.创建socket地址addr：IP，端口，类型
 * 2.socket()创建一个socket对象
 * 3.bind(addr)绑定端口
 * 4.listen()监听
 */
bool WebServer::InitSocket_(){
    int ret = 0;
    // 创建socket
    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(port_);
    // 延迟关闭
    struct linger optLinger = {0};
    if (openLinger_){
        optLinger.l_onoff = 1;
        optLinger.l_linger = 1;
    }
    listenFd_ = socket(AF_INET, SOCK_STREAM, 0);
    // 设置延迟关闭
    ret = setsockopt(listenFd_,SOL_SOCKET,SO_LINGER,&optLinger, sizeof(optLinger));
    if (ret == -1) {
        LOG_ERROR("set socket setsockopt error !");
        close(listenFd_);
        return false;
    }
    // 设置端口复用
    int optval = 1;
    ret = setsockopt(listenFd_,SOL_SOCKET,SO_REUSEADDR,(const void*)&optval, sizeof(int));
    // 绑定端口
    ret = bind(listenFd_, (struct sockaddr*)&addr, sizeof(addr));
    // 设置监听，创建连接队列
    ret = listen(listenFd_, 6);
    epoller_->AddFd(listenFd_, listenEvent_ | EPOLLIN);
    SetFdNonblock(listenFd_); // 设置非阻塞
    LOG_INFO("Server port:%d", port_);
    return true;
}
/**
 * InitEventMode(int trigMode):设置水平触发/边缘触发
 * case 0:监听socket-LT 连接socket-LT ：简单应用，开发调试
 * case 1:监听socket-LT 连接socket-ET ：高并发-处理大量数据请求
 * case 2:监听socket-ET 连接socket-LT ：高并发-处理大量连接请求
 * case 3:监听socket-ET 连接socket-ET ：全部高并发
 */
void WebServer::InitEventMode_(int trigMode){
    listenEvent_ = EPOLLRDHUP;  // 检测对端关闭
    connEvent_ = EPOLLRDHUP | EPOLLONESHOT; // 检测对端关闭+单次事件监听(保证事件只被一个线程处理)
    switch(trigMode){
        case 0:  // 全水平触发LT
        break;
    case 1:
        connEvent_ |= EPOLLET; // 连接事件ET
        break;
    case 2:
        listenEvent_ |= EPOLLET;
        break;
    case 3:
        listenEvent_ |= EPOLLET;
        connEvent_ |= EPOLLET;
        break;
    default:
        listenEvent_ |= EPOLLET;
        connEvent_ |= EPOLLET;
        break;
    }
    // 设置静态标记，让所有http连接知道当下的模式
    HttpConn::isET = (connEvent_ & EPOLLET);
}

/**
 * AddClient: 新增一个用户连接
 * 1.给这个fd创建一个http对象
 * 2.给这个fd创建一个timer定时器
 * 3.添加epoll监听，设置非阻塞
 */
void WebServer::AddClient(int fd, sockaddr_in addr){
    users_[fd].init(fd, addr);
    if (timeoutMS_ > 0) {
        timer_->add(fd, timeoutMS_, std::bind(&WebServer::CloseConn_,this,&users_[fd]));
    }
    epoller_->AddFd(fd, EPOLLIN | connEvent_);
    SetFdNonblock(fd);
    LOG_INFO("Client[%d] in!", users_[fd].GetFd());
}

/**
 * DealListen: listenFd触发了监听，给新连接创建socket对象
 * 如果是LT模式，listenFd会不断触发监听，直到accept取出所有新连接
 * 如果是ET模式，listenFd由于设置非阻塞，accept不会阻塞，失败返回-1
 */
void WebServer::DealListen_(){
    struct sockaddr_in addr;
    socklen_t len = sizeof(addr);
    do {
        int fd = accept(listenFd_,(struct sockaddr*)&addr, &len);
        if (fd == -1) {
            // 没有连接了
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                break;
            }
            // 出错了
            LOG_ERROR("accept error: %s", strerror(errno));
        } else if (HttpConn::userCount >= MAX_FD) {
            // 连接超限制了
            SendError_(fd, "Server busy!");
            LOG_WARN("Clients is full!");
            break;
        }
        AddClient(fd, addr);
    } while (listenEvent_ & EPOLLET);
}
void WebServer::DealWrite_(HttpConn* client){
    ExtentTime_(client);
    threadpool_->AddTask(std::bind(&WebServer::OnWrite_, this, client));
}
void WebServer::DealRead_(HttpConn* client){
    ExtentTime_(client);
    threadpool_->AddTask(std::bind(&WebServer::OnRead_, this, client));
}

void WebServer::SendError_(int fd, const char* info){
    int ret = send(fd, info, strlen(info), 0);
    if (ret < 0){
        LOG_WARN("send error to client[%d] error!", fd);
    }
    close(fd);
}
void WebServer::ExtentTime_(HttpConn* client){
    if (timeoutMS_ > 0){
        timer_->adjust(client->GetFd(), timeoutMS_);
    }
}
void WebServer::CloseConn_(HttpConn* client){
    LOG_INFO("Client[%d] quit!", client->GetFd());
    epoller_->DelFd(client->GetFd());
    client->Close();
}

/**
 * OnRead：读取数据
 * 交给http-client读数据，client会判断是否为ET模式
 */
void WebServer::OnRead_(HttpConn* client){
    int Errno = 0;
    ssize_t ret = client->read(&Errno);
    if (ret < 0 && Errno != EAGAIN) {
        // 断开连接了
        CloseConn_(client);
        return;
    }
    LOG_DEBUG("Read request to Buffer finish!");
    OnProcess_(client);
}
void WebServer::OnWrite_(HttpConn* client){
    int Errno = 0;
    ssize_t ret = client->write(&Errno);
    if (client->ToWriteBytes() == 0) {
        // 传输完成
        if (client->IsKeepAlive()) {
            OnProcess_(client);
            return;
        }
    } else if (ret < 0) {
        if (Errno == EAGAIN) {  // 缓冲区满
            // 继续传输
            epoller_->ModFd(client->GetFd(), connEvent_ | EPOLLOUT);
            return;
        }
    }
    CloseConn_(client);
}
void WebServer::OnProcess_(HttpConn* client){
    if (client->process()) {
        // 请求处理完成，需要发送响应 -> 监听fd可写事件
        epoller_->ModFd(client->GetFd(), connEvent_ | EPOLLOUT);
    } else {
        // 请求处理未完成，需要继续读取请求 -> 监听fd可读事件
        epoller_->ModFd(client->GetFd(), connEvent_ | EPOLLIN);
    }
}

int WebServer::SetFdNonblock(int fd){
    assert(fd > 0);
    return fcntl(fd, F_SETFL, fcntl(fd, F_GETFD, 0) | O_NONBLOCK);
}