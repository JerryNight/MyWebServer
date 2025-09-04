#include "../http/httpresponse.h"
#include <filesystem>
#include <iostream>
#include <unistd.h>

void TestHttpResponse(){
    HttpResponse response;
    // 获取当前路径(在项目根路径编译->根路径)
    std::filesystem::path curr_path;
    curr_path = std::filesystem::current_path();
    std::string srcDir = curr_path.string();
    srcDir += "/resources/";
    std::string path = "index.html";

    response.init(srcDir, path, true);
    Buffer writebuffer;
    response.MakeResponse(writebuffer);
    std::cout << writebuffer.RetrieveAllToString()<< std::endl;
    //std::cout << "file():" << response.File() << std::endl;
    //std::cout << "filelen():" << response.FileLen() << std::endl;
}

int main(){
    TestHttpResponse();
}