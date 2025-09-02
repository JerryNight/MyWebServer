#include "../http/httprequest.h"

#include <iostream>

void TestHttpRequest(){
    Log::GetInstance()->init(0, "./log", ".log", 5000);
    Buffer buffer;
    // std::string request = "GET /index.html HTTP/1.1\r\n"
    //                 "Host: www.example.com\r\n"
    //                 "User-Agent: Mozilla/5.0 (Windows NT 10.0; Win64; x64)\r\n"
    //                 "Accept: text/html,application/xhtml+xml\r\n"
    //                 "Accept-Language: zh-CN,zh;q=0.9,en;q=0.8\r\n"
    //                 "Accept-Encoding: gzip, deflate\r\n"
    //                 "Connection: keep-alive\r\n"
    //                 "\r\n";
    std::string request = "POST /login HTTP/1.1\r\n"
                        "Host: www.example.com\r\n"
                        "User-Agent: Mozilla/5.0 (Windows NT 10.0; Win64; x64)\r\n"
                        "Content-Type: application/x-www-form-urlencoded\r\n"
                        "Content-Length: 29\r\n"
                        "Accept: text/html,application/xhtml+xml\r\n"
                        "Connection: keep-alive\r\n"
                        "\r\n"
                        "username=lyt&password=996430\r\n";
    buffer.Append(request);
    HttpRequest http;
    http.parse(buffer);
    std::cout << "path:" << http.path() << std::endl;
    std::cout << "merhod:" << http.method() << std::endl;
    std::cout << "version:" << http.version() << std::endl;
    std::cout << "isKeepAlive:" << http.IsKeepAlive() << std::endl;
    std::cout << "------------------" << std::endl;
    for (auto && item : http.header_){
        std::cout << item.first << ":" << item.second << std::endl;
    }
    std::cout << "------------------" << std::endl;
    std::cout << "username:" << http.GetPost("username") << std::endl;
    std::cout << "password:" << http.GetPost("password") << std::endl;
}

void TestHttpRequestLine(){
    Buffer buffer;
    std::string request = "GET /index.html HTTP/1.1\r\n";
    HttpRequest http;
    http.ParseRequestLine_(request);
    std::cout << "path:" << http.path() << std::endl;
    std::cout << "merhod:" << http.method() << std::endl;
    std::cout << "version:" << http.version() << std::endl;
    std::cout << "state:" << http.state_ << std::endl;
}

void TestHttpHeader(){
    Buffer buffer;
    std::string request = "Host: www.example.com";
                        // "User-Agent: Mozilla/5.0 (Windows NT 10.0; Win64; x64)\r\n"
                        // "Accept: text/html,application/xhtml+xml\r\n"
                        // "Accept-Language: zh-CN,zh;q=0.9,en;q=0.8\r\n"
                        // "Accept-Encoding: gzip, deflate\r\n"
                        // "Connection: keep-alive\r\n"
                        // "\r\n";
    HttpRequest http;
    http.ParseHeader_(request);
    for (auto && item : http.header_){
        std::cout << item.first << ":" << item.second << std::endl;
    }
    std::cout << "state:" << http.state_ << std::endl;
}

int main(){
    TestHttpRequest();
}