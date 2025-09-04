#include "httpconn.h"

bool HttpConn::isET;
const char* HttpConn::srcDir;
std::atomic<int> HttpConn::userCount;

/**
 * 1. httpconn:read() 从fd中读数据到readbuffer中(会自动扩容)
 *      readbuffer中的数据：http请求数据
 * 2. httpconn:process() 使用request解析http请求：request.parse(readbuffer)
 *      解析的资源路径保存在request中的path数据成员里。
 *      response将响应数据写到writebuffer中，还有要发给客户端的文件数据，用iovec保存
 * 3. httpconn:write() 向fd写数据，iovec包含writebuffer和文件数据
 */

HttpConn::HttpConn(){
    fd_ = -1;
    addr_ = {0};
    isClose_ = true;
}
HttpConn::~HttpConn(){
    Close();
}
void HttpConn::init(int sockfd, const sockaddr_in & addr){
    assert(sockfd > 0);
    userCount++;
    addr_ = addr;
    fd_ = sockfd;
    readBuffer_.RetrieveAll();
    writeBuffer_.RetrieveAll();
    isClose_ = false;
    LOG_INFO("Client[%d](%s:%d) in, userCount:%d", fd_, GetIP(), GetPort(), (int)userCount);
}

ssize_t HttpConn::read(int* saveError){
    ssize_t len = -1;
    do {
        len = readBuffer_.ReadFd(fd_, saveError);
        if (len <= 0)
            break;
    } while (isET); // 边缘触发，要一次读完
    return len;
}
ssize_t HttpConn::write(int* saveError){
    ssize_t len = -1;
    do {
        len = writev(fd_, iov_, iovCount_);
        if (len <= 0){
            *saveError = errno;
            break;
        }
        if (iov_[0].iov_len + iov_[1].iov_len == 0)
            break; // 写完了
        // 一次写，iov[0]还没写完
        if (static_cast<size_t>(len) <= iov_[0].iov_len) {
            iov_[0].iov_base = (char*)iov_[0].iov_base + len;
            iov_[0].iov_len -= len;
            writeBuffer_.Retrieve(len);
        } else { // iov[0] 写完了，iov[1]还没写完
            iov_[1].iov_base = (char*) iov_[1].iov_base + (len - iov_[0].iov_len);
            iov_[1].iov_len -= (len - iov_[0].iov_len);
            if(iov_[0].iov_len) {
                writeBuffer_.RetrieveAll();
                iov_[0].iov_len = 0;
            }
        }
    } while (isET || ToWriteBytes() > 10240);
    return len;
}

void HttpConn::Close(){
    response_.UnmapFile();
    if (!isClose_) {
        isClose_ = true;
        userCount--;
        close(fd_);
        LOG_INFO("Client[%d](%s:%d) quit, UserCount:%d", fd_, GetIP(), GetPort(), (int)userCount);
    }
}

int HttpConn::GetFd() const{
    return fd_;
}
int HttpConn::GetPort() const{
    return addr_.sin_port;
}
const char* HttpConn::GetIP() const{
    return inet_ntoa(addr_.sin_addr);
}
sockaddr_in HttpConn::GetAddr() const{
    return addr_;
}

// bool HttpConn::process(){
//     request_.init();
//     if (readBuffer_.ReadableBytes() <= 0)
//         return false;
//     if (request_.parse(readBuffer_)) {
//         LOG_DEBUG("%s", request_.path().c_str());
//         response_.init(srcDir, request_.path(), request_.IsKeepAlive(), 200);
//     } else {
//         response_.init(srcDir, request_.path(), false, 400);
//     }
//     // 向writebuffer里写入http响应数据-页面
//     response_.MakeResponse(writeBuffer_);
//     // 响应头
//     iov_[0].iov_base = const_cast<char*>(writeBuffer_.Peek());
//     iov_[0].iov_len = writeBuffer_.ReadableBytes();
//     iovCount_ = 1;
//     // 文件数据
//     if (response_.File() && response_.FileLen() > 0) {
//         iov_[1].iov_base = response_.File();
//         iov_[1].iov_len = response_.FileLen();
//         iovCount_ = 2;
//     }
//     LOG_DEBUG("filesize:%d, %d  to %d", response_.FileLen() , iovCount_, ToWriteBytes());
//     return true;
// }

bool HttpConn::process(){
    LOG_DEBUG("HttpConn::process:Begin~")
    request_.init();
    if (readBuffer_.ReadableBytes() <= 0)
        return false;
    // 用while循环，如果buffer一次收到多个请求，可以循环解析
    while (readBuffer_.ReadableBytes() > 0) {
        // 没有解析完，说明请求发送不完整，等待下次读，再解析
        if (!request_.parse(readBuffer_)){
            return false;
        }
        // 解析成功
        if (request_.path().empty()){
            response_.init(srcDir, request_.path(), false, 400);
            LOG_DEBUG("HttpConn::process: path is empty");
        }
        LOG_DEBUG("HttpConn::process: %s", request_.path().c_str());
        response_.init(srcDir, request_.path(), request_.IsKeepAlive(), 200);
        response_.MakeResponse(writeBuffer_);
        // 响应头
        iov_[0].iov_base = const_cast<char*>(writeBuffer_.Peek());
        iov_[0].iov_len = writeBuffer_.ReadableBytes();
        iovCount_ = 1;
        // 文件数据
        if (response_.File() && response_.FileLen() > 0) {
            iov_[1].iov_base = response_.File();
            iov_[1].iov_len = response_.FileLen();
            iovCount_ = 2;
        }
        LOG_DEBUG("HttpConn::process:filesize:%d, %d  to %d", response_.FileLen() , iovCount_, ToWriteBytes());

        // 如果是短连接，直接返回
        if (!request_.IsKeepAlive()){
            break;
        }
        // 如果是长连接，继续解析buffer，看看是否有新的请求
        request_.init();
    }
    // buffer里所有数据解析完成，返回true
    return true;
}