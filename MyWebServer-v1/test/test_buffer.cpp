#include "../buffer/buffer.h"
#include <iostream>

void TestBuffer() {
    Buffer buffer;
    //const char* name = "liyongtao";
    std::string str1("liyongtao");
    buffer.Append(str1);
    std::string str("123456789");
    buffer.Append(str);
    std::cout << "buffer.peek(): " << buffer.Peek() << std::endl;
    std::cout << "buffer.WritableBytes(): " << buffer.WritableBytes() << std::endl;
    std::cout << "buffer.ReadableBytes(): " << buffer.ReadableBytes() << std::endl;
    std::cout << "buffer.PrependableBytes(): " << buffer.PrependableBytes() << std::endl;
    buffer.Retrieve(10);
    std::cout << "buffer.peek(): " << buffer.Peek() << std::endl;
    std::cout << "buffer.WritableBytes(): " << buffer.WritableBytes() << std::endl;
    std::cout << "buffer.ReadableBytes(): " << buffer.ReadableBytes() << std::endl;
    std::cout << "buffer.PrependableBytes(): " << buffer.PrependableBytes() << std::endl;
    // 测试扩容
    std::cout << "-----------------" << std::endl;
    std::string str2(1024, 'y');
    buffer.Append(str2); // 18 + 1024 + 1 = 1043
    std::cout << "buffer.WritableBytes(): " << buffer.WritableBytes() << std::endl;
    std::cout << "buffer.ReadableBytes(): " << buffer.ReadableBytes() << std::endl;
    std::cout << "buffer.PrependableBytes(): " << buffer.PrependableBytes() << std::endl;
}

int main() {
    TestBuffer();
}