#ifndef BUFFER_H
#define BUFFER_H
#include <iostream>
#include <vector>
#include <atomic>
#include <string>

/**
 * 网络编程缓冲池，用于在TCP链接中高效地收发数据：由vector<char>构成缓冲池。
 * 核心功能：1.读写位置管理  2.自动扩容  3.支持readv分散读  4.支持字符串操作
 *  0------------[read_pos][][][][][][][][write_pos]-------------buf.size()
 */

class Buffer {
public:
    Buffer(int buffer_size = 1024);
    ~Buffer() = default;
    // 当前缓冲区状态信息
    size_t WritableBytes() const;  // 可写数据大小   [write_pos, buf.size()]
    size_t ReadableBytes() const;  // 可读数据大小   [read_pos,   write_pos]
    size_t PrependableBytes() const; // 已读，可复用空间大小  [0,  read_pos]

    const char* Peek() const;           // 获取可读数据的起始位置
    void EnsureWritable(size_t length); // 确保缓冲区有length空间大小，不够会调用makespace
    void HasWritten(size_t length);     // 写完数据后，把writepos指针前移

    // 读相关接口
    void Retrieve(size_t length);       // 消费length个字节
    void RetrieveUntil(const char* end);// 消费到指定位置
    void RetrieveAll();                 // 消费所有，清空缓冲区
    std::string RetrieveAllToString();  // 转成string，再清空

    // 写相关接口
    char* BeginWrite();                  // 获取写指针（指向可写位置）
    const char* BeginWriteConst() const;
    void Append(const std::string& str); // 向缓冲区写数据
    void Append(const char* str, size_t length);
    void Append(const void* data, size_t length);
    void Append(const Buffer& buff);

    // 网络IO接口
    ssize_t ReadFd(int fd, int* save_errno);
    ssize_t WriteFd(int fd, int* save_errno);

private:
    char* BeginPtr_();  // buffer 起始位置
    const char* BeginPtr_() const;
    void MakeSpace_(size_t len);

    std::vector<char> buffer_;
    std::atomic<std::size_t> read_pos_;  // 读位置
    std::atomic<std::size_t> write_pos_; // 写位置

};




#endif