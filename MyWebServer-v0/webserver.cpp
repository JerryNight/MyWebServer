#include "webserver.h"



WebServer::WebServer(){
    // http连接池
    users = new http_connection[MAX_FD];
    // root 文件夹路径
    char server_path[200];
    getcwd(server_path, 200);
    char Root[6] = "/root";
    root = (char *)malloc(strlen(server_path) + strlen(Root) + 1);
    strcpy(root, server_path); // 当前项目路径
    strcat(root, Root);       // 拼接root文件夹
    // 没有http都对应一个定时器
    users_timer = new client_data[MAX_FD];
}
WebServer::~WebServer(){
    close(epollfd);
    close(listenfd);
    close(pipefd[1]);
    close(pipefd[0]);
    delete[] users;
    delete[] users_timer;
    delete threadPool;
}
    // 初始化
void WebServer::init(int _port , string _user, string _passWord, string _databaseName,
              int _log_write , int _opt_linger, int _trigmode, int _sql_num,
              int _thread_num, int _close_log, int _actor_model)
{
    port = _port;
    username = _user;
    password = _passWord;
    databaseName = _databaseName;
    logWrite = _log_write;
    optLinger = _opt_linger;
    trigMode = _trigmode;
    sqlConnNum = _sql_num;
    threadNum = _thread_num;
    closeLog = _close_log;
    actorModel = _actor_model;
}
// 线程池初始化
void WebServer::thread_pool(){
    threadPool = new threadpool<http_connection>(actorModel, sqlConnPool,threadNum);
}
// 数据库连接池初始化
void WebServer::sql_pool(){
    // 初始化数据库
    sqlConnPool = sql_connection_pool::get_instance();
    sqlConnPool->init("localhost",username,password,databaseName,3306,sqlConnNum,closeLog);
    // 读用户信息表
    users->initmysql_result(sqlConnPool);
}
// 开启日志
void WebServer::log_write(){
    // 开启日志
    if (closeLog == 0) {
        if (logWrite == 1) {
            Log::get_instance().init("./ServerLog", closeLog, 2000, 800000, 800);
        } else {
            Log::get_instance().init("./ServerLog", closeLog, 2000, 800000, 0);
        }
    }
}
// 设置触发模式
void WebServer::trig_mode(){
    if (trigMode == 0) {
        // LT + LT
        listenTrigMode = 0;
        connectTrigMode = 0;
    } else if (trigMode == 1) {
        // LT + ET
        listenTrigMode = 1;
        connectTrigMode = 0;
    } else if (trigMode == 2) {
        // ET + LT
        listenTrigMode = 0;
        connectTrigMode = 1;
    } else if (trigMode == 3) {
        // ET + ET
        listenTrigMode = 1;
        connectTrigMode = 1;
    }
}
// 开启事件监听
void WebServer::event_listen(){
    listenfd = socket(PF_INET, SOCK_STREAM, 0);
    assert(listenfd >= 0);
    // 是否延迟关闭连接
    if (optLinger == 1) {
        struct linger tmp = {1, 1};
        setsockopt(listenfd, SOL_SOCKET, SO_LINGER, &tmp, sizeof(tmp));
    } else if (optLinger == 0){
        struct linger tmp = {0, 1};
        setsockopt(listenfd, SOL_SOCKET, SO_LINGER, &tmp, sizeof(tmp));
    }
    // 网络编程
    int ret = 0;
    struct sockaddr_in address;
    bzero(&address, sizeof(address));
    address.sin_family = AF_INET;
    address.sin_port = htons(port);
    address.sin_addr.s_addr = htonl(INADDR_ANY);
    int flag = 1;
    setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &flag, sizeof(flag));
    ret = bind(listenfd,(struct sockaddr*)&address, sizeof(address));
    assert(ret >= 0);
    ret = listen(listenfd, 5);
    assert(ret >= 0);
    // 设置工具类epoll_utils :定时时间/epollfd/pipefd
    epoll_utils.init(TIMESLOT);
    epoll_util::u_epollfd = epollfd;
    epoll_util::u_pipe_fd = pipefd;
    // epoll创建内核事件表
    // struct epoll_event event[MAX_EVENT_NUMBER];
    epollfd = epoll_create(1);
    assert(epollfd != -1);
    epoll_utils.add_fd(epollfd, listenfd, false, listenTrigMode);  // 监听listenfd
    // http连接池设置epollfd
    http_connection::_epoll_fd = epollfd;
    // 创建本地socket，主线程与子线程通信（线程间通信）
    ret = socketpair(PF_UNIX, SOCK_STREAM, 0, pipefd);
    assert(ret != -1);
    epoll_utils.set_nonBlocking(pipefd[1]); // 写不阻塞
    epoll_utils.add_fd(epollfd, pipefd[0], false, 0);   // 监听本地socket
    epoll_utils.add_sig(SIGPIPE, SIG_IGN);  // 信号处理：忽略SIGPIPE
    epoll_utils.add_sig(SIGALRM, epoll_utils.sig_handler, false); // sig_handler会向pipe写一条数据
    epoll_utils.add_sig(SIGTERM, epoll_utils.sig_handler, false); 
    // 定时五秒钟
    alarm(TIMESLOT);
}
// 给fd创建定时器
void WebServer::timer(int connectfd, struct sockaddr_in client_address){
    users[connectfd].init(connectfd,client_address,root,trigMode,closeLog,username,password,databaseName);
    // 初始化每个http连接的定时器
    users_timer[connectfd].address = client_address;
    users_timer[connectfd].sockfd = connectfd;
    // 创建一个timeNode
    timeNode * timer = new timeNode;
    timer->user_data = &users_timer[connectfd];
    timer->cb_func = cb_func;
    time_t cur = time(NULL);
    timer->expire = cur + 3 * TIMESLOT;
    users_timer[connectfd].timer = timer;
    epoll_utils.timer_lst.add_timer(timer);

}
// fd的定时器向后延迟
void WebServer::adjust_timer(timeNode *timer){
    time_t cur = time(NULL);
    timer->expire = cur + 3 * TIMESLOT;
    epoll_utils.timer_lst.adjust_timer(timer);
    LOG_INFO("%s", "adjust timer once");
}
// 定时器
void WebServer::deal_timer(timeNode *timer, int sockfd){
    // 删除监听、关闭sockfd
    timer->cb_func(&users_timer[sockfd]);
    if (timer) epoll_utils.timer_lst.del_timer(timer);
    LOG_INFO("close fd %d", users_timer[sockfd].sockfd);
}
// 收到客户端连接时的处理
bool WebServer::deal_client_data(){
    struct sockaddr_in client_address;
    socklen_t client_addrlength = sizeof(client_data);
    if (listenTrigMode == 0) { // 水平触发
        // 获取客户端
        int connfd = accept(listenfd,(struct sockaddr*)&client_address,&client_addrlength);
        if (connfd < 0){
            LOG_ERROR("%s:errno is:%d", "accept error", errno);
            return false;
        }
        // 如果连接数超过上限
        if (http_connection::_user_count >= MAX_FD){
            epoll_utils.show_error(connfd, "Internal server busy");
            LOG_ERROR("%s", "Internal server busy");
            return false;
        }
        // 给这个客户度创建一个定时器
        timer(connfd, client_address);
    } else { 
        // 边缘触发
        for (;;) {
            int connfd = accept(listenfd,(struct sockaddr*)&client_address,&client_addrlength);
            if (connfd < 0){
                LOG_ERROR("%s:errno is:%d", "accept error", errno);
            return false;
            }
            // 如果连接数超过上限
            if (http_connection::_user_count >= MAX_FD){
                epoll_utils.show_error(connfd, "Internal server busy");
                LOG_ERROR("%s", "Internal server busy");
                break;
            }
            // 给这个客户度创建一个定时器
            timer(connfd, client_address);
        }
        return false;
    }
    return true;
}
// 收到信号，处理信号
bool WebServer::deal_with_signal(bool& timeout, bool& stop_server){
    int ret = 0;
    // int sig;
    char signals[1024];
    ret = recv(pipefd[0], signals, sizeof(signals), 0);
    if (ret == 0 || ret == -1) return false;
    for (int i = 0; i < ret; i++){
        switch(signals[i]){
            case SIGALRM:
                timeout = true;
                break;
            case SIGTERM:
                stop_server = true;
                break;
        }
    }
    return true;
}
// 处理读数据请求
void WebServer::deal_with_read(int sockfd){
    timeNode* timer = users_timer[sockfd].timer;
    if (trigMode == 1) { // reactor模式
        if (timer) adjust_timer(timer);
        // 如果检测到读事件，放入请求队列
        threadPool->append(&users[sockfd], 0);
        for (;;) {
            if (users[sockfd].improv == 1){
                if (users[sockfd].timer_flag == 1){
                    deal_timer(timer, sockfd);
                    users[sockfd].timer_flag = 0;
                }
                users[sockfd].improv = 0;
                break;
            }
        }
    } else { // proactor 模式
        if (users[sockfd].read_once()) {
            LOG_INFO("deal with the client(%s)", inet_ntoa(users[sockfd].get_address()->sin_addr));
            // 如果检测到读事件，放入请求队列
            threadPool->append(&users[sockfd], 0);
            if (timer) adjust_timer(timer);
        } else {
            deal_timer(timer, sockfd);
        }
    }
}
// 处理写数据请求
void WebServer::deal_with_write(int sockfd){
    timeNode* timer = users_timer[sockfd].timer;
    if (trigMode == 1) { // reactor模式
        if (timer) adjust_timer(timer);
        // 如果检测到读事件，放入请求队列
        threadPool->append(&users[sockfd], 0);
        for (;;) {
            if (users[sockfd].improv == 1){
                if (users[sockfd].timer_flag == 1){
                    deal_timer(timer, sockfd);
                    users[sockfd].timer_flag = 0;
                }
                users[sockfd].improv = 0;
                break;
            }
        }
    } else { // proactor模式
        if (users[sockfd].write()) {
            LOG_INFO("send data to the client(%s)", inet_ntoa(users[sockfd].get_address()->sin_addr));
            if (timer) adjust_timer(timer);
        } else {
            deal_timer(timer, sockfd);
        }
    }
}
// 开启循环监听
void WebServer::event_loop(){
    bool timeout = false;
    bool stop_server = false;
    while (!stop_server) {
        int number = epoll_wait(epollfd, events, MAX_EVENT_NUMBER, -1);
        if (number < 0 && errno != EINTR){
            LOG_ERROR("%s", "epoll falied");
            break;
        }
        for (int i = 0; i < number; i++){
            int sockfd = events[i].data.fd;
            // 有新的客户端进来了
            if (listenfd == sockfd){
                if (deal_client_data() == false) continue;
            } else if (events[i].events & (EPOLLRDHUP | EPOLLHUP | EPOLLERR)){
                // 该连接出错了
                timeNode* timer = users_timer[sockfd].timer;
                deal_timer(timer, sockfd);
            } else if ((sockfd == pipefd[0]) && (events[i].events & EPOLLIN)){
                // 信号，且可读。检查是否要关闭服务器
                if (deal_with_signal(timeout, stop_server) == false){
                    LOG_ERROR("%s", "dealclientdata failure");
                }
            } else if (events[i].events & EPOLLIN) {
                deal_with_read(sockfd);
            } else if (events[i].events & EPOLLOUT) {
                deal_with_write(sockfd);
            }
        }
        // 定时器开着
        if (timeout) {
            epoll_utils.timer_handler();
            LOG_INFO("%s", "timer tick");
            timeout = false;
        }
    }
}