/*
 * @Author: HejieY 2145381080@qq.com
 * @Date: 2026-10-13 19:32:26
 * @FilePath: /muduo-http-server/source/server.hpp
 * @Description: 这个类是高性能服务器的缓冲区,核心思想是使用vector<char>来存储数据,双指针维护空间
 * @Questions: 
 * 1.为什么类内设计时都是用地址而不用下标？：系统调用参数为地址，用地址方便与系统调用协调
 * 2.取地址返回的是指针,强转uint64_t就可以拿到具体的地址
 * 3.void* 加法是未定义问题,步长是不确定的
 * Copyright (c) 2026 by HeJieY, All Rights Reserved. 
 */

//


#include <cstdint>
#include<iostream>
#include <sys/types.h>
#include<vector>
#include<cstdint>
#define BUFFER_SIZE 1024
class Buffer
{
public:
    Buffer():_read_index(0),_write_index(0),_capacity(BUFFER_SIZE),_buffer(_capacity) {}
    ~Buffer() {}
    ssize_t Write()
    {

    }
    ssize_t Read()
    {

    }
    bool Clear()
    {
        
    }
    uint64_t GetReadableSize() 
    {
        return _write_index - _read_index;
    }
    
private:
    //老师的项目中将char* 写成了void*，查询资料得知void*的加法是未定义的，g++编译器把其看成1，考虑到这些，我们用char*
    char* GetBegin() { return &(*_buffer.begin());}
    char* GetReadPos() { return GetBegin() + _read_index; }
    char* GetWritePos() { return GetBegin() + _write_index;}
    uint64_t GetWriteTailMem() { return _capacity - _write_index; }
    uint64_t GetTotalMem() { return GetWriteTailMem() + GetReadFrontMem(); }
    uint64_t GetReadFrontMem() { return _read_index; }
     bool EnsureMemEnough(uint64_t len)
    {
        if(len <= GetWriteTailMem()) { return true; }
        //进行空间的挪移
        if(GetTotalMem() >= len && len > GetWriteTailMem()) {}
        //进行合理的扩容和空间的挪移
        else { }
    }

private:
    uint64_t _read_index;    
    uint64_t _write_index;
    uint64_t _capacity;
    std::vector<char> _buffer; 
    
};

