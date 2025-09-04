#include "httprequest.h"
#include <regex>


const std::unordered_set<std::string> HttpRequest::DEFAULT_HTML{
    "/index", "/register", "/login",
    "/welcome", "/video", "/picture"
};

const std::unordered_map<std::string, int> HttpRequest::DEFAULT_HTML_TAG{
    {"/register.html", 0}, {"/login.html", 1}
};

bool HttpRequest::UserVerify_(const std::string& username,const std::string& password,bool isLogin){
    if (username == "" || password == "") return false;
    LOG_INFO("HttpRequest::UserVerify: username:%s, password=%s",username.c_str(),password.c_str());
    MYSQL* sql;
    // SqlPool::GetInstance()->init("127.0.0.1",3306,"root","Lyt996430$","webserverdb",10);
    SqlPoolRAII raii(&sql, SqlPool::GetInstance());
    assert(sql);

    char query[1024] = {0}; // sql查询
    MYSQL_RES* result = nullptr;
    MYSQL_ROW row;
    bool ret;

    // 查询用户密码
    snprintf(query, 2024, "select username,passwd from user where username = '%s' limit 1;", username.c_str());
    LOG_DEBUG("%s", query);
    if (mysql_query(sql, query)){
        LOG_ERROR("HttpRequest::UserVerify:mysql query failed: %s",mysql_error(sql));
        return false;
    }
    result = mysql_store_result(sql);
    if (result == nullptr) {
        LOG_DEBUG("HttpRequest::UserVerify:query has no result");
        return false;
    }
    row = mysql_fetch_row(result);
    if (row) {
        std::string db_username = row[0] ? row[0] : "";
        std::string db_password = row[1] ? row[1] : "";
        // 已有账号，登录
        if (isLogin) {
            if (db_password == password){
                LOG_DEBUG("HttpRequest::UserVerify:Login Success!");
                ret = true;
            } else {
                LOG_DEBUG("HttpRequest::UserVerify:Password Mismatch!");
                ret = false;
            }
        } else { // 已有账号，注册
            LOG_DEBUG("HttpRequest::UserVerify:User Exist!");
            ret = false;
        }
    } else {
        if (isLogin) { // 账号不存在
            LOG_DEBUG("HttpRequest::UserVerify:login:%s not exist",username.c_str());
            ret = false;
        } else { // 注册账号
            LOG_DEBUG("HttpRequest::UserVerify:register!");
            memset(query,0, 1024);
            snprintf(query,1024,"insert into user(username,passwd) values('%s','%s');",username.c_str(),password.c_str());
            LOG_DEBUG("HttpRequest::UserVerify:%s",query);
            if (mysql_query(sql, query)){
                LOG_DEBUG("HttpRequest::UserVerify:MYSQL insert error");
                return false;
            }
            ret = true;
        }
    }
    mysql_free_result(result);
    return ret;
}
// 把16进制字符转成10进制
int HttpRequest::ConverHex(char ch){
    if (ch >= '0' && ch <= '9') return ch - '0';
    if (ch >= 'A' && ch <= 'F') return ch - 'A' + 10;
    if (ch >= 'a' && ch <= 'f') return ch - 'a' + 10;
    return ch;
}

void HttpRequest::init(){
    method_ = path_ = version_ = body_ = "";
    state_ = REQUEST_LINE;
    header_.clear();
    post_.clear();
    contentLen_ = 0;
}



//  <method> <path> HTTP/<version>
// 解析请求行
bool HttpRequest::ParseRequestLine_(const std::string& line){
    std::regex pattern("^([^ ]*) ([^ ]*) HTTP/([^ ]*)$");
    std::smatch match;
    if (std::regex_match(line, match, pattern)) {
        method_ = match[1];
        path_ = match[2];
        version_ = match[3];
        LOG_DEBUG("ParseRequestLine:[%s], [%s], [%s]", method_.c_str(), path_.c_str(), version_.c_str());
        return true;
    }
    LOG_ERROR("Parse Request Line Error: %s", line);
    // printf("Parse Request Line Error: %s\n", line.c_str());
    return false;
}

void HttpRequest::ParseHeader_(const std::string& line){
    std::regex pattern("^([^:]*): ?(.*)$");
    std::smatch match;
    if (std::regex_match(line, match, pattern)){
        std::string key = match[1];
        std::string value = match[2];
        while (!value.empty() && (value.front() == ' ' || value.front() == '\t'))
            value.erase(value.begin());
        while (!value.empty() && (value.back() == ' ' || value.back() == '\t'))
            value.pop_back();
        header_[key] = value;
        if (key == "Content-Length")
            contentLen_ = static_cast<size_t>(std::stoul(value));
    } else {
        LOG_DEBUG("ParseHeader finish!");
        if (line.empty() && header_.count("Content-Length") != 0){
            state_ = BODY;
        } else {
            state_ = FINISH;
        }
    }
}

void HttpRequest::ParseBody_(const std::string& line){
    LOG_DEBUG("ParseBody start~");
    body_ = line;
    ParsePost_();
    state_ = FINISH;
    LOG_DEBUG("Body:%s, len:%d",line.c_str(),line.size());
}

// 解析请求路径
void HttpRequest::ParsePath_(){
    if (path_ == "/") {
        path_ += "index.html";
    } else {
        for (auto& item : DEFAULT_HTML) {
            if (item == path_){
                path_ += ".html";
                break;
            }
        }
    }
    LOG_DEBUG("ParsePath:%s",path_.c_str());
}

// 解析请求方式
void HttpRequest::ParsePost_(){
    LOG_DEBUG("Method = %s && Content-Type = %s", method_.c_str(), header_["Content-Type"].c_str());
    if (method_ == "POST" && header_["Content-Type"] == "application/x-www-form-urlencoded"){
        ParseFromUrl_();
        // 是register或login
        if (DEFAULT_HTML_TAG.count(path_)) {
            int tag = DEFAULT_HTML_TAG.find(path_)->second;
            LOG_DEBUG("Tag:%d",tag);
            if (tag == 0 || tag == 1){
                bool islogin = (tag == 1);
                if (UserVerify_(post_["username"], post_["password"], islogin)){
                    path_ = "/welcome.html";
                }else{
                    path_ = "/error.html";
                }
            }
        }
    }
}

// url编码解析
// 原始：name=John Doe&city=New York
// 编码：name=John%20Doe&city=New%20York
void HttpRequest::ParseFromUrl_(){
    if (body_.size() == 0) return;
    // 1. 处理百分比解码
    std::string result;
    result.reserve(body_.size());
    for (size_t i = 0; i < body_.size(); i++) {
        if (body_[i] == '%' && i + 2 < body_.size()) {
            // 解码
            int hexValue = ConverHex(body_[i + 1]) * 16 + ConverHex(body_[i + 2]);
            result += static_cast<char>(hexValue);
            i += 2;
        } else if(body_[i] == '+') {
            result += ' ';
        } else {
            result += body_[i];
        }
    }
    // 2. 通过&符解析k-v
    size_t start = 0, end = 0;
    while (end <= result.size()) {
        if (result[end] == '&' || end == result.size()) {
            if (start < end){
                std::string pairStr(result.data() + start, end - start);
                auto [key, value] = ParseKeyValue_(pairStr);
                // post_[std::move(key)] = std::move(value);
                post_[key] = value;
                LOG_DEBUG("Parse:%s = %s", key.c_str(), value.c_str());
            }
            start = end + 1;
        }
        end++;
    }
}

std::pair<std::string, std::string> HttpRequest::ParseKeyValue_(std::string pairStr){
    size_t pos = pairStr.find("=");
    if (pos == std::string::npos) {
        // 没有找到=
        return {pairStr, ""};
    }
    std::string key = pairStr.substr(0, pos);
    std::string value = pairStr.substr(pos + 1);
    return {std::move(key), std::move(value)};
}

/**
 * 1.解析http请求行，解析出path/method/version等
 * 2.解析http请求头，解析出key-value,保存在header_
 * 3.解析http请求体，parseBody -> parsePost -> parseUrl -> 是否登录 ->UserVerify
 */
bool HttpRequest::parse(Buffer& buffer){
    const char CRLF[] = "\r\n";
    if (buffer.ReadableBytes() <= 0)
        return false;
    while (buffer.ReadableBytes() && state_ != FINISH) {
        // 在buffer可读字符串中，找到第一个\r\n
        const char* lineEnd = std::search(buffer.Peek(),buffer.BeginWriteConst(),CRLF,CRLF+2);
        // 没有在buffer里找到\r\n，说明数据不完整。
        if (lineEnd == buffer.BeginWrite() && buffer.ReadableBytes() < contentLen_){
            // 没读完
            LOG_DEBUG("TCP packet splicing! lineEnd = %s, buffer = %s", lineEnd, buffer.Peek());
            LOG_DEBUG("ContentLen = %d, buffer = ", contentLen_ , buffer.ReadableBytes());
            return false;
        }
        // 获取一行
        std::string line(buffer.Peek(), lineEnd);
        // 解析
        switch (state_){
        case REQUEST_LINE:
            if (!ParseRequestLine_(line)){
                // 把buffer打印出来
                std::string p = buffer.RetrieveAllToString();
                LOG_ERROR("Parse Error:%s", p.c_str());
                return false;
            }
            ParsePath_();
            state_ = HEADER;
            buffer.RetrieveUntil(lineEnd + 2);
            break;
        case HEADER:
            ParseHeader_(line);
            buffer.RetrieveUntil(lineEnd + 2);
            break;
        case BODY:
            LOG_DEBUG("HttpRequest::parse: Buffer.ReadableBytes() = %d; contentLen = %d", buffer.ReadableBytes(), contentLen_);
            ParseBody_(line);
            buffer.Retrieve(contentLen_);
            break;
        default:
            break;
        }
        // 消费buffer
        
    }
    LOG_DEBUG("[%s], [%s], [%s]", method_.c_str(), path_.c_str(), version_.c_str());
    return true;
}

std::string HttpRequest::path() const{
    return path_;
}

std::string& HttpRequest::path(){
    return path_;
}
std::string HttpRequest::method() const {
    return method_;
}

std::string HttpRequest::version() const {
    return version_;
}

std::string HttpRequest::GetPost(const std::string& key) const {
    assert(key != "");
    if(post_.count(key) == 1) {
        return post_.find(key)->second;
    }
    return "";
}

std::string HttpRequest::GetPost(const char* key) const {
    assert(key != nullptr);
    if(post_.count(key) == 1) {
        return post_.find(key)->second;
    }
    return "";
}

bool HttpRequest::IsKeepAlive() const {
    if(header_.count("Connection") == 1) {
        return header_.find("Connection")->second == "keep-alive" && version_ == "1.1";
    }
    return false;
}