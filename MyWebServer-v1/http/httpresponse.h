#ifndef HTTP_RESPONSE_H
#define HTTP_RESPONSE_H

#include "../buffer/buffer.h"
#include "../log/log.h"
#include <unordered_map>
#include <string>
#include <fcntl.h>  // open
#include <unistd.h> // close
#include <sys/stat.h> //stat
#include <sys/mman.h> // mmap munmap
#include <assert.h>

class HttpResponse {
public:
    HttpResponse();
    ~HttpResponse();

    // 初始化资源路径
    void init(const std::string& srcDir, std::string& path, bool isKeepAlive = false, int code = -1);
    void MakeResponse(Buffer& buffer);
    void UnmapFile();
    char* File();
    size_t FileLen() const;
    void ErrorContent(Buffer& buffer, std::string message);
    int Code() const { return code_; }

private:
    void AddStateLine_(Buffer& buffer);
    void AddHeader_(Buffer& buffer);
    void AddContent_(Buffer& buffer);

    void ErrorHtml_();
    std::string GetFileType_();

private:
    int code_; // 返回码
    bool isKeepAlive_;
    std::string path_;   // 资源路径
    std::string srcDir_; // 资源根路径
    char* mmFile_;
    struct stat mmFileStat_;

    static const std::unordered_map<std::string, std::string> SUFFIX_TYPE;
    static const std::unordered_map<int, std::string> CODE_STATUS;
    static const std::unordered_map<int, std::string> CODE_PATH;
};


#endif