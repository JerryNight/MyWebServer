#include "buffer.h"
#include <cassert>  // assert
#include <sys/uio.h> // struct iovec  writev
#include <unistd.h> // read write


Buffer::Buffer(int buffer_size):buffer_(buffer_size),read_pos_(0),write_pos_(0){}
// 当前缓冲区状态信息
// 可写数据大小   [write_pos, buf.size()]
size_t Buffer::WritableBytes() const {
    return buffer_.size() - write_pos_;
}
// 可读数据大小   [read_pos,   write_pos]
size_t Buffer::ReadableBytes() const {
    return write_pos_ - read_pos_;
}
// 已读，可复用空间大小  [0,  read_pos]
size_t Buffer::PrependableBytes() const {
    return read_pos_;
}
// 获取可读数据的起始位置
const char* Buffer::Peek() const {
    return buffer_.data() + read_pos_;
}
// 确保缓冲区有length空间大小，不够会调用makespace
void Buffer::EnsureWritable(size_t length) {
    // 如果空间不够了
    if (WritableBytes() < length) {
        MakeSpace_(length);
    }
    assert(WritableBytes() > length);
}
// 写完数据后，把writepos指针前移
void Buffer::HasWritten(size_t length){
    write_pos_ += length;
}

// 读相关接口
// 消费length个字节
void Buffer::Retrieve(size_t length) {
    assert((write_pos_ - read_pos_ - length) > 0);
    read_pos_ += length;
}
// 消费到指定位置
void Buffer::RetrieveUntil(const char* end){
    assert(Peek() < end);
    Retrieve(end - Peek());
}
// 消费所有，清空缓冲区
void Buffer::RetrieveAll() {
    read_pos_ = 0;
    write_pos_ = 0;
}
// 转成string，再清空
std::string Buffer::RetrieveAllToString() {
    std::string str(Peek(), ReadableBytes());
    RetrieveAll();
    return str;
}

// 写相关接口
// 获取写指针（指向可写位置）
char* Buffer::BeginWrite() {
    return BeginPtr_() + write_pos_;
}
// 能在const buffer 对象上调用（只读）
const char* Buffer::BeginWriteConst() const {
    return BeginPtr_() + write_pos_;
}
// 向缓冲区写数据
void Buffer::Append(const std::string& str) {
    Append(str.c_str(), str.size());
}
void Buffer::Append(const char* str, size_t length) {
    assert(str); // 不为空
    // 空间不够了
    if (WritableBytes() < length) {
        MakeSpace_(length);
    }
    std::copy(str, str + length, BeginWrite());
    HasWritten(length);
}
void Buffer::Append(const void* data, size_t length){
    assert(data);
    Append(static_cast<const char*>(data), length);
}
void Buffer::Append(const Buffer& buff) {
    Append(buff.Peek(),buff.WritableBytes());
}

// 网络IO接口
// 从sockfd中读数据，用struct iovec 一次把数据读完
ssize_t Buffer::ReadFd(int fd, int* save_errno){
    char buf[65535]; // 如果buffer空间不够，就读到buf里，后续再复制到buffer
    struct iovec iov[2];
    iov[0].iov_base = BeginWrite();
    iov[0].iov_len = WritableBytes();
    iov[1].iov_base = buf;
    iov[1].iov_len = sizeof(buf);
    ssize_t len = readv(fd, iov, 2);
    if (len < 0) {
        *save_errno = errno;
    } else if (static_cast<size_t>(len) <= WritableBytes()) {
        HasWritten(len);
    } else {
        Append(buf, len - WritableBytes());
    }
    return len;
}
// 向sockfd中写入buffer可读的数据
ssize_t Buffer::WriteFd(int fd, int* save_errno){
    ssize_t len = write(fd, Peek(), ReadableBytes());
    if (len < 0) {
        *save_errno = errno;
    }
    read_pos_ += len;
    return len;
}


char* Buffer::BeginPtr_() {
    return buffer_.data();
}
const char* Buffer::BeginPtr_() const {
    return buffer_.data();
}
void Buffer::MakeSpace_(size_t len) {
    // 如果空间不够，扩容
    if ((WritableBytes() + PrependableBytes()) < len) {
        buffer_.resize(write_pos_ + len + 1);
    }
    size_t read_size = ReadableBytes();
    std::copy(Peek(), Peek() + read_size, BeginPtr_());
    read_pos_ = 0;
    write_pos_ = read_pos_ + read_size;
    assert(ReadableBytes() == read_size);
}

