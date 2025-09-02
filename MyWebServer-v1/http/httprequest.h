#ifndef HTTP_REQUEST_H
#define HTTP_REQUEST_H

#include <string>
#include <unordered_map>
#include <unordered_set>
#include <cstring>
#include <utility> // pair
//#include <algorithm> // search
#include "../buffer/buffer.h"
#include "../log/log.h"
#include "../sqlpool/sqlpoolRAII.h"


class HttpRequest {
public:
    enum PARSE_STATE {
        REQUEST_LINE,
        HEADER,
        BODY,
        FINISH,
    };

    enum HTTP_CODE {
        NO_REQUEST = 0,
        GET_REQUEST,
        BAD_REQUEST,
        NO_RESOURCE,
        FORBIDDENT_REQUEST,
        FILE_REQUEST,
        INTERNAL_ERROR,
        CLOSED_CONNECTION,
    };

public:
    HttpRequest(){init();}
    ~HttpRequest() = default;

    void init();
    bool parse(Buffer& buffer);

    std::string path() const;
    std::string& path();
    std::string method() const;
    std::string version() const;
    std::string GetPost(const std::string& key) const;
    std::string GetPost(const char* key) const;

    bool IsKeepAlive() const;

public:
    // 解析一行数据
    bool ParseRequestLine_(const std::string& line);
    // 解析请求头
    void ParseHeader_(const std::string& line);
    // 解析请求体
    void ParseBody_(const std::string& line);

    void ParsePath_();
    void ParsePost_();
    void ParseFromUrl_();
    std::pair<std::string, std::string> ParseKeyValue_(std::string pairStr);
    static bool UserVerify_(const std::string& username,  const std::string& password,bool isLogin);

    PARSE_STATE state_;
    std::string method_, path_, version_, body_;
    std::unordered_map<std::string, std::string> header_;
    std::unordered_map<std::string, std::string> post_;

    static const std::unordered_set<std::string> DEFAULT_HTML;
    static const std::unordered_map<std::string, int> DEFAULT_HTML_TAG;
    static int ConverHex(char ch);
};





#endif