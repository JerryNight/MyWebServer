#include "httpresponse.h"


const std::unordered_map<std::string, std::string> HttpResponse::SUFFIX_TYPE = {
    { ".html",  "text/html" },
    { ".xml",   "text/xml" },
    { ".xhtml", "application/xhtml+xml" },
    { ".txt",   "text/plain" },
    { ".rtf",   "application/rtf" },
    { ".pdf",   "application/pdf" },
    { ".word",  "application/nsword" },
    { ".png",   "image/png" },
    { ".gif",   "image/gif" },
    { ".jpg",   "image/jpeg" },
    { ".jpeg",  "image/jpeg" },
    { ".au",    "audio/basic" },
    { ".mpeg",  "video/mpeg" },
    { ".mpg",   "video/mpeg" },
    { ".avi",   "video/x-msvideo" },
    { ".gz",    "application/x-gzip" },
    { ".tar",   "application/x-tar" },
    { ".css",   "text/css "},
    { ".js",    "text/javascript "},
};
const std::unordered_map<int, std::string> HttpResponse::CODE_STATUS = {
    { 200, "OK" },
    { 400, "Bad Request" },
    { 403, "Forbidden" },
    { 404, "Not Found" },
};
const std::unordered_map<int, std::string> HttpResponse::CODE_PATH = {
    { 400, "/400.html" },
    { 403, "/403.html" },
    { 404, "/404.html" },
};

HttpResponse::HttpResponse(){
    code_ = -1;
    path_ = srcDir_ = "";
    isKeepAlive_ = false;
    mmFile_ = nullptr;
    mmFileStat_ = {0};
}
HttpResponse::~HttpResponse(){
    UnmapFile();
}

void HttpResponse::init(const std::string& srcDir, std::string& path, bool isKeepAlive, int code){
    LOG_DEBUG("HttpResponse::init begin~");
    assert(srcDir != "");
    if(mmFile_) UnmapFile();
    code_ = code;
    path_ = path;
    srcDir_ = srcDir;
    mmFile_ = nullptr;
    mmFileStat_ = {0};
    isKeepAlive_ = isKeepAlive;
}
void HttpResponse::MakeResponse(Buffer& buffer){
    LOG_DEBUG("HttpResponse::MakeResponse begin~");
    // 判断资源文件
    if (stat((srcDir_ + path_).data(), &mmFileStat_) == -1 || S_ISDIR(mmFileStat_.st_mode)){
        // 找不到该文件，或文件类型是路径，返回码为404（找不到资源）
        code_ = 404;
    } else if (!(mmFileStat_.st_mode & S_IROTH)) {
        // 检查文件权限 read - other
        code_ = 403;
    } else if (code_ == -1) {
        code_ = 200;
    }
    ErrorHtml_();
    AddStateLine_(buffer);
    AddHeader_(buffer);
    AddContent_(buffer);
}
void HttpResponse::UnmapFile(){
    if (mmFile_) {
        munmap(mmFile_, mmFileStat_.st_size);
        mmFile_ = nullptr;
    }
}
char* HttpResponse::File(){
    return mmFile_;
}
size_t HttpResponse::FileLen() const{
    return mmFileStat_.st_size;
}

void HttpResponse::ErrorContent(Buffer& buffer, std::string message){
    std::string body;
    std::string status;
    body += "<html><title>Error</title>";
    body += "<body bgcolor=\"ffffff\">";
    if (CODE_STATUS.count(code_) == 1) {
        status = CODE_STATUS.find(code_)->second;
    } else {
        status = "Bad Request";
    }
    body += std::to_string(code_) + " : " + status  + "\n";
    body += "<p>" + message + "</p>";
    body += "<hr><em>MyWebServer</em></body></html>";

    buffer.Append("Content-length: " + std::to_string(body.size()) + "\r\n\r\n");
    buffer.Append(body);
}

void HttpResponse::AddStateLine_(Buffer& buffer){
    std::string status;
    if(CODE_STATUS.count(code_) == 1) {
        status = CODE_STATUS.find(code_)->second;
    }
    else {
        code_ = 400;
        status = CODE_STATUS.find(400)->second;
    }
    buffer.Append("HTTP/1.1 " + std::to_string(code_) + " " + status + "\r\n");
}
void HttpResponse::AddHeader_(Buffer& buffer){
    buffer.Append("Connection: ");
    if(isKeepAlive_) {
        buffer.Append("keep-alive\r\n");
        buffer.Append("keep-alive: max=6, timeout=120\r\n");
    } else{
        buffer.Append("close\r\n");
    }
    buffer.Append("Content-type: " + GetFileType_() + "\r\n");
}
void HttpResponse::AddContent_(Buffer& buffer){
    int srcfd = open((srcDir_ + path_).data(), O_RDONLY);
    if (srcfd < 0) {
        ErrorContent(buffer, "File Not Found!");
        return;
    }
    // 将文件映射到内存
    LOG_DEBUG("file path %s", (srcDir_ + path_).data());
    void* ret = mmap(NULL, mmFileStat_.st_size,PROT_READ, MAP_PRIVATE,srcfd,0);
    if (ret == MAP_FAILED) {
        ErrorContent(buffer, "File not Found!");
        return;
    }
    mmFile_ = (char*)ret;
    close(srcfd);
    buffer.Append("Content-length: " + std::to_string(mmFileStat_.st_size) + "\r\n\r\n");
}

void HttpResponse::ErrorHtml_(){
    if(CODE_PATH.count(code_) == 1) {
        path_ = CODE_PATH.find(code_)->second;
        stat((srcDir_ + path_).data(), &mmFileStat_);
    }
}
std::string HttpResponse::GetFileType_(){
    size_t index = path_.find_first_of('.');
    if (index == std::string::npos) {
        return "text/plain";
    }
    std::string suffix = path_.substr(index);
    if (SUFFIX_TYPE.count(suffix) == 1) {
        return SUFFIX_TYPE.find(suffix)->second;
    }
    return "text/plain";
}
