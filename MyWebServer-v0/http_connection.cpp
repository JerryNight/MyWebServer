#include "http_connection.h"

//#define connfdET //边缘触发非阻塞
#define connfdLT //水平触发阻塞

//#define listenfdET //边缘触发非阻塞
#define listenfdLT //水平触发阻塞

//定义http响应的一些状态信息
const char *ok_200_title = "OK";
const char *error_400_title = "Bad Request";
const char *error_400_form = "Your request has bad syntax or is inherently impossible to staisfy.\n";
const char *error_403_title = "Forbidden";
const char *error_403_form = "You do not have permission to get file form this server.\n";
const char *error_404_title = "Not Found";
const char *error_404_form = "The requested file was not found on this server.\n";
const char *error_500_title = "Internal Error";
const char *error_500_form = "There was an unusual problem serving the request file.\n";

locker g_lock; // 全局锁
map<string, string> g_user_map; // 全局用户信息表

int http_connection::_epoll_fd = -1;
int http_connection::_user_count = 0;

// 全局方法 -------------------------------------------------------------------------

int setnonblocking(int fd){
    return fcntl(fd, F_SETFL, fcntl(fd, F_GETFL) | O_NONBLOCK);
}
// 添加监听
void addfd(int epollfd, int fd, bool one_shot, int TRIGMode){
    struct epoll_event event;
    event.data.fd = fd;
    if (TRIGMode == 1){ // 边缘触发
        event.events = EPOLLIN | EPOLLET | EPOLLRDHUP;
    } else { // 水平触发
        event.events = EPOLLIN | EPOLLRDHUP;
    }
    if (one_shot) event.events |= EPOLLONESHOT;
    epoll_ctl(epollfd, EPOLL_CTL_ADD, fd, &event);
    setnonblocking(fd);
}
// 移除监听
void removefd(int epollfd, int fd){
    epoll_ctl(epollfd, EPOLL_CTL_DEL, fd, 0);
    close(fd);
}
// 修改属性
void modfd(int epollfd, int fd, int ev, int TRIGMode){
    epoll_event event;
    event.data.fd = fd;
    if (TRIGMode == 1){
        event.events = ev | EPOLLET | EPOLLONESHOT | EPOLLRDHUP;
    } else {
        event.events = ev | EPOLLONESHOT | EPOLLRDHUP;
    }
    epoll_ctl(epollfd, EPOLL_CTL_MOD, fd, &event);
}

// 公共接口 --------------------------------------------------------------------------------

void http_connection::init(int sockfd, const sockaddr_in &addr, char *root, int TRIGMode,
                     int close_log, string user, string passwd, string sqlname){
    _sockfd = sockfd; // 客户端fd
    _address = addr;  // 客户端地址
    // 添加监听，总用户数量++
    addfd(_epoll_fd, sockfd, true, TRIGMode);
    _user_count++;
    // 保存sql信息
    strcpy(_sql_user, user.c_str());
    strcpy(_sql_passwd, passwd.c_str());
    strcpy(_sql_name, sqlname.c_str());
    doc_root = root;
    _trigMode = TRIGMode;
    closeLog = close_log;

    init();
}
void http_connection::close_conn(bool real_close){
    if (real_close && _sockfd != -1) {
        removefd(_epoll_fd, _sockfd);
        _sockfd = -1;
        _user_count--;
    }
}
void http_connection::process(){
    HTTP_CODE read_ret = process_read();
    if (read_ret == NO_REQUEST)
    {
        modfd(_epoll_fd, _sockfd, EPOLLIN, _trigMode);
        return;
    }
    bool write_ret = process_write(read_ret);
    if (!write_ret)
    {
        close_conn();
    }
    modfd(_epoll_fd, _sockfd, EPOLLOUT, _trigMode);
}
bool http_connection::read_once(){
    // 缓冲区读完了
    if (_read_idx >= READ_BUFFER_SIZE) {
        return false;
    }
    // 开始读
    int bytes_read = 0;
    if (_trigMode == 0) { // 水平模式
        // 读满buf
        bytes_read = recv(_sockfd, _read_buf + _read_idx, READ_BUFFER_SIZE - _read_idx, 0);
        _read_idx += bytes_read;
        if (bytes_read <= 0) 
            return false;
        else 
            return true; // 读成功，可能未读完。水平模式还会继续来读。
    } else {
        // 边缘模式：必须读完socket_buf里的内容。有读缓冲区满的风险。
        for (;;) {
            bytes_read = recv(_sockfd, _read_buf + _read_idx, READ_BUFFER_SIZE - _read_idx, 0);
            // bytes_read == 0：对方关闭连接
            if (bytes_read == 0) {
                return false;
            } else if (bytes_read == -1) {
                // 读完了
                if (errno == EAGAIN || errno == EWOULDBLOCK) {
                    break;
                } else {
                    return false;
                }
            }
            _read_idx += bytes_read;
        }
        return true;
    }
}
// 向客户端socket写数据
bool http_connection::write(){
    int temp = 0;
    // 写完了，没东西写了。
    if (bytes_to_send == 0){
        // 这次请求的任务完成，清理http信息
        modfd(_epoll_fd, _sockfd, EPOLLIN, _trigMode);  // 监听sockfd的可读事件
        init();
        return true;
    }
    for (;;) {
        // 向客户端socket缓冲区写数据
        temp = writev(_sockfd, _iv, _iv_count);
        if(temp < 0){
            if (errno == EAGAIN){  // 
                modfd(_epoll_fd, _sockfd, EPOLLOUT, _trigMode); // 监听sockfd的可写事件
                return true;
            }
            unmap();
            return false;
        }
        bytes_have_send += temp;
        bytes_to_send -= temp;
        if (bytes_have_send >= _iv[0].iov_len){
            _iv[0].iov_len = 0;
            _iv[1].iov_base = _file_address + (bytes_have_send - _write_idx);
            _iv[1].iov_len = bytes_to_send;
        } else {
            _iv[0].iov_base = _write_buf + bytes_have_send;
            _iv[0].iov_len = _iv[0].iov_len - bytes_have_send;
        }
        // 数据写完！重新设置监听读事件
        if (bytes_to_send <= 0) {
            unmap();
            modfd(_epoll_fd, _sockfd, EPOLLIN, _trigMode);
            if (_linger) {
                init();
                return true;
            } else {
                return false;
            }
        }
    }
}
void http_connection::initmysql_result(sql_connection_pool *connPool){
    // 从sql连接池取一个:创建一个RAII就行了
    sql_connection_RAII sql_conn_RAII(&mysql, connPool);
    // sql 查询
    const char* query = "select username,passwd from user";
    mysql_query(mysql, query);
    // 检索结果集
    MYSQL_RES* result = mysql_store_result(mysql);
    int num_fields = mysql_num_fields(result);
    MYSQL_FIELD* fields = mysql_fetch_field(result);
    while (MYSQL_ROW row = mysql_fetch_row(result)){
        string tmp1(row[0]);
        string tmp2(row[1]);
        g_user_map[tmp1] = tmp2;
    }
}

// 私有方法 ----------------------------------------------------------------------------
void http_connection::init(){ // 初始化一个http连接
    mysql = NULL;
    bytes_to_send = 0;
    bytes_have_send = 0;
    _check_state = CHECK_STATE_REQUESTLINE; // 当前解析状态，设为解析一行
    _linger = false;   // 不设置延迟断开
    _method = GET;   // 默认GET请求类型
    _url = 0;
    _version = 0;
    _content_length = 0;
    _host = 0;
    _start_line = 0;
    _checked_idx = 0;
    _read_idx = 0;
    _write_idx = 0;
    cgi = 0;
    _state = 0; // 0读
    timer_flag = 0;
    improv = 0;

    memset(_read_buf, '\0', READ_BUFFER_SIZE);
    memset(_write_buf, '\0', WRITE_BUFFER_SIZE);
    memset(_real_file, '\0', FILENAME_LEN);
}
// 解析一行
http_connection::LINE_STATUS http_connection::parse_line(){
    for (char temp = '0'; _checked_idx < _read_idx; _checked_idx++){
        temp = _read_buf[_checked_idx];
        if (temp == '\r') {  // \r 回车，要跟\n换行一起出现才行。
            if (_read_buf[_checked_idx + 1] == '\n'){
                // 一行结束了
                _read_buf[_checked_idx++] = '\0'; // \r 替换为\0
                _read_buf[_checked_idx++] = '\0'; // \n 替换为\0
                return LINE_OK;
            } else if((_checked_idx + 1) == _read_idx){
                // 缓冲区读完了,但没有读到 \n 说明还有数据在sockfd里没读
                return LINE_OPEN;
            }
            // 单独遇到一个\r，出错了
            return LINE_BAD;
        } else if (temp == '\n'){
            if (_checked_idx > 1 && _read_buf[_checked_idx - 1] == '\r'){
                // 之前遇到\r没读完的
                _read_buf[_checked_idx - 1] = '\0'; // \r 替换为\0
                _read_buf[_checked_idx++] = '\0'; // \n 替换为\0
                return LINE_OK;
            }
            return LINE_BAD;
        }
    }
    return LINE_OPEN;
}
// POST http://localhost/hello/index.html HTTP/1.1
http_connection::HTTP_CODE http_connection::parse_request_line(char *text){
    _url = strpbrk(text, " \t"); // 在Text中找第一个空格或\t，
    if (!_url) return BAD_REQUEST;
    *_url++ = '\0'; // 把这个位置置空
    // 解析最前面的method
    char* method = text; 
    if (strcasecmp(method, "GET") == 0){
        _method = GET;
    } else if (strcasecmp(method, "POST") == 0){
        _method = POST;
        cgi = 1;
    } else {
        return BAD_REQUEST;
    }
    // 解析version
    _url += strspn(_url, " \t"); //跳过url中的空白符
    _version = strpbrk(_url, " \t"); // 最后一个空白符的位置
    if (!_version) return BAD_REQUEST;
    *_version++ = '\0';
    _version += strspn(_version, " \t"); // 跳过空白字符
    if (strcasecmp(_version, "HTTP/1.1") != 0) return BAD_REQUEST;
    // 解析url
    if (strncasecmp(_url, "http://", 7) == 0){
        _url += 7;
        _url = strchr(_url, '/');
    }
    if (strncasecmp(_url, "https://", 8) == 0){
        _url += 8;
        _url = strchr(_url, '/');
    }
    if (!_url || _url[0] != '/') return BAD_REQUEST;
    if (strlen(_url) == 1){
        // 显示judge.html页面
        strcat(_url, "judge.html");
    }
    _check_state = CHECK_STATE_HEADER;
    return NO_REQUEST;
}
// 解析请求头，会调用多次
http_connection::HTTP_CODE http_connection::parse_headers(char *text){
    if (text[0] == '\0'){
        // 解析结束
        if (_content_length != 0){
            _check_state = CHECK_STATE_CONTENT;
            return NO_REQUEST;
        }
        return GET_REQUEST;
    } else if (strncasecmp(text, "Connection:", 11) == 0){
        text += 11;
        text += strspn(text, " \t");
        if (strcasecmp(text, "keep-alive") == 0){
            _linger = true;
        }
    } else if (strncasecmp(text, "Content-length:", 15) == 0){
        text += 15;
        text += strspn(text, " \t");
        _content_length = atol(text);
    } else if (strncasecmp(text, "Host:", 5) == 0){
        text += 5;
        text += strspn(text, " \t");
        _host = text;
    } else{
        LOG_INFO("oop!unknow header: %s", text);
    }
    return NO_REQUEST;
}
// 解析请求体
http_connection::HTTP_CODE http_connection::parse_content(char *text){
    // 缓冲区数据大小 >= 已check的 + 未check的。说明请求体在缓冲池中。
    if (_read_idx >= (_content_length + _checked_idx))
    {
        // 最后一个字符设置空白字符
        text[_content_length] = '\0';
        //POST请求中最后为输入的用户名和密码
        _string = text;
        return GET_REQUEST;
    }
    return NO_REQUEST;
}
http_connection::HTTP_CODE http_connection::do_request(){
    strcpy(_real_file, doc_root);
    int len = strlen(doc_root);
    // p指向最后一个/: 0注册，1首页，2登录，3注册，5.图片，6.视频 7.fans
    const char *p = strrchr(_url, '/');
    //处理cgi
    if (cgi == 1 && (*(p + 1) == '2' || *(p + 1) == '3')){
        //根据标志判断是登录检测还是注册检测
        char flag = _url[1];

        char *_url_real = (char *)malloc(sizeof(char) * 200);
        strcpy(_url_real, "/");
        strcat(_url_real, _url + 2);
        strncpy(_real_file + len, _url_real, FILENAME_LEN - len - 1);
        free(_url_real);

        //将用户名和密码提取出来
        //user=123&passwd=123
        char name[100], password[100];
        int i;
        for (i = 5; _string[i] != '&'; ++i)
            name[i - 5] = _string[i];
        name[i - 5] = '\0';

        int j = 0;
        for (i = i + 10; _string[i] != '\0'; ++i, ++j)
            password[j] = _string[i];
        password[j] = '\0';

        if (*(p + 1) == '3')
        {
            //如果是注册，先检测数据库中是否有重名的
            //没有重名的，进行增加数据
            char *sql_insert = (char *)malloc(sizeof(char) * 200);
            strcpy(sql_insert, "INSERT INTO user(username, passwd) VALUES(");
            strcat(sql_insert, "'");
            strcat(sql_insert, name);
            strcat(sql_insert, "', '");
            strcat(sql_insert, password);
            strcat(sql_insert, "')");

            if (g_user_map.find(name) == g_user_map.end())
            {
                g_lock.lock();
                int res = mysql_query(mysql, sql_insert);
                g_user_map.insert(pair<string, string>(name, password));
                g_lock.unlock();

                if (!res)
                    strcpy(_url, "/log.html");
                else
                    strcpy(_url, "/registerError.html");
            }
            else
                strcpy(_url, "/registerError.html");
        }
        //如果是登录，直接判断
        //若浏览器端输入的用户名和密码在表中可以查找到，返回1，否则返回0
        else if (*(p + 1) == '2')
        {
            if (g_user_map.find(name) != g_user_map.end() && g_user_map[name] == password)
                strcpy(_url, "/welcome.html");
            else
                strcpy(_url, "/logError.html");
        }
    }

    if (*(p + 1) == '0')
    {
        char *_url_real = (char *)malloc(sizeof(char) * 200);
        strcpy(_url_real, "/register.html");
        strncpy(_real_file + len, _url_real, strlen(_url_real));

        free(_url_real);
    }
    else if (*(p + 1) == '1')
    {
        char *_url_real = (char *)malloc(sizeof(char) * 200);
        strcpy(_url_real, "/log.html");
        strncpy(_real_file + len, _url_real, strlen(_url_real));

        free(_url_real);
    }
    else if (*(p + 1) == '5')
    {
        char *_url_real = (char *)malloc(sizeof(char) * 200);
        strcpy(_url_real, "/picture.html");
        strncpy(_real_file + len, _url_real, strlen(_url_real));

        free(_url_real);
    }
    else if (*(p + 1) == '6')
    {
        char *_url_real = (char *)malloc(sizeof(char) * 200);
        strcpy(_url_real, "/video.html");
        strncpy(_real_file + len, _url_real, strlen(_url_real));

        free(_url_real);
    }
    else if (*(p + 1) == '7')
    {
        char *_url_real = (char *)malloc(sizeof(char) * 200);
        strcpy(_url_real, "/fans.html");
        strncpy(_real_file + len, _url_real, strlen(_url_real));

        free(_url_real);
    }
    else
        strncpy(_real_file + len, _url, FILENAME_LEN - len - 1);

    if (stat(_real_file, &_file_stat) < 0)
        return NO_RESOURCE;

    if (!(_file_stat.st_mode & S_IROTH))
        return FORBIDDEN_REQUEST;

    if (S_ISDIR(_file_stat.st_mode))
        return BAD_REQUEST;

    int fd = open(_real_file, O_RDONLY);
    _file_address = (char *)mmap(0, _file_stat.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
    close(fd);
    return FILE_REQUEST;

}
http_connection::HTTP_CODE http_connection::process_read(){
    LINE_STATUS line_status = LINE_OK;
    HTTP_CODE ret = NO_REQUEST;
    char *text = 0;

    while ((_check_state == CHECK_STATE_CONTENT && line_status == LINE_OK) || ((line_status = parse_line()) == LINE_OK))
    {
        text = get_line();
        _start_line = _checked_idx;
        LOG_INFO("%s", text);
        switch (_check_state)
        {
        case CHECK_STATE_REQUESTLINE:
        {
            ret = parse_request_line(text);
            if (ret == BAD_REQUEST)
                return BAD_REQUEST;
            break;
        }
        case CHECK_STATE_HEADER:
        {
            ret = parse_headers(text);
            if (ret == BAD_REQUEST)
                return BAD_REQUEST;
            else if (ret == GET_REQUEST)
            {
                return do_request();
            }
            break;
        }
        case CHECK_STATE_CONTENT:
        {
            ret = parse_content(text);
            if (ret == GET_REQUEST)
                return do_request();
            line_status = LINE_OPEN;
            break;
        }
        default:
            return INTERNAL_ERROR;
        }
    }
    return NO_REQUEST;
}
bool http_connection::process_write(HTTP_CODE ret){
    switch (ret)
    {
    case INTERNAL_ERROR:
    {
        add_status_line(500, error_500_title);
        add_headers(strlen(error_500_form));
        if (!add_content(error_500_form))
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
        add_status_line(200, ok_200_title);
        if (_file_stat.st_size != 0)
        {
            add_headers(_file_stat.st_size);
            _iv[0].iov_base = _write_buf;
            _iv[0].iov_len = _write_idx;
            _iv[1].iov_base = _file_address;
            _iv[1].iov_len = _file_stat.st_size;
            _iv_count = 2;
            bytes_to_send = _write_idx + _file_stat.st_size;
            return true;
        }
        else
        {
            const char *ok_string = "<html><body></body></html>";
            add_headers(strlen(ok_string));
            if (!add_content(ok_string))
                return false;
        }
    }
    default:
        return false;
    }
    _iv[0].iov_base = _write_buf;
    _iv[0].iov_len = _write_idx;
    _iv_count = 1;
    bytes_to_send = _write_idx;
    return true;
}
void http_connection::unmap(){
    if (_file_address){
        munmap(_file_address, _file_stat.st_size);
        _file_address = 0;
    }
}
bool http_connection::add_response(const char *format, ...){
    if (_write_idx >= WRITE_BUFFER_SIZE) return false;
    va_list arg_list;
    va_start(arg_list, format);
    int len = vsnprintf(_write_buf + _write_idx, WRITE_BUFFER_SIZE - 1 - _write_idx, format, arg_list);
    if (len >= (WRITE_BUFFER_SIZE - 1 - _write_idx))
    {
        va_end(arg_list);
        return false;
    }
    _write_idx += len;
    va_end(arg_list);
    LOG_INFO("request:%s", _write_buf);
    return true;
}
bool http_connection::add_content(const char *content){
    return add_response("%s", content);
}
bool http_connection::add_status_line(int status, const char *title){
    return add_response("%s %d %s\r\n", "HTTP/1.1", status, title);
}
bool http_connection::add_headers(int content_length){
    return add_content_length(content_length) && add_linger() && add_blank_line();
}
bool http_connection::add_content_type(){
    return add_response("Content-Type:%s\r\n", "text/html");
}
bool http_connection::add_content_length(int content_length){
    return add_response("Content-Length:%d\r\n", content_length);
}
bool http_connection::add_linger(){
    return add_response("Connection:%s\r\n", (_linger == true) ? "keep-alive" : "close");
}
bool http_connection::add_blank_line(){
    return add_response("%s", "\r\n");
}