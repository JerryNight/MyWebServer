#ifndef HTTP_CONN_H
#define HTTP_CONN_H
#include <arpa/inet.h> // sockaddr_in
#include <sys/types.h>
#include <sys/uio.h>  // readv writev
#include <stdlib.h>   // atoi()
#include <atomic>     // atomic
#include "../buffer/buffer.h"
#include "../log/log.h"
#include "../sqlpool/sqlpoolRAII.h"
#include "httprequest.h"
#include "httpresponse.h"

class HttpConn {
public:
    HttpConn();
    ~HttpConn();
    void init(int sockfd, const sockaddr_in & addr);

    ssize_t read(int* saveError);
    ssize_t write(int* saveError);

    void Close();

    int GetFd() const;
    int GetPort() const;
    const char* GetIP() const;
    sockaddr_in GetAddr() const;

    bool process();
    int ToWriteBytes() { return iov_[0].iov_len + iov_[1].iov_len; }
    bool IsKeepAlive() const { return request_.IsKeepAlive(); }

    static bool isET;
    static const char* srcDir;
    static std::atomic<int> userCount;

private:
    int fd_;
    struct sockaddr_in addr_;
    bool isClose_;
    int iovCount_;
    struct iovec iov_[2];

    Buffer readBuffer_;
    Buffer writeBuffer_;

    HttpRequest request_;
    HttpResponse response_;
};


#endif