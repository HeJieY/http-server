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
#include<cstdio>
#include<cstring>
#include<ctime>
#include<cstring>
#include<algorithm>
#include <cstdint>
#include<iostream>
#include <sys/types.h>
#include<vector>
#include<cstdint>
#include<cassert>
#define BUFFER_SIZE 1024
#define LOG_LEVEL INF

#define LOG(level, fmt, ...)                                           \
    do {                                                               \
        if (level >= LOG_LEVEL) {                                      \
            time_t t = time(NULL);                                     \
            struct tm *tm_info = localtime(&t);                        \
            char time_buf[9];                                          \
            strftime(time_buf, sizeof(time_buf), "%H:%M:%S", tm_info); \
            fprintf(stdout, "[%s] %s:%d " fmt "\n",                   \
                    time_buf, __FILE__, __LINE__, ##__VA_ARGS__);      \
        }                                                              \
    } while (0)

#define INF_LOG(fmt, ...) LOG(INF, fmt, ##__VA_ARGS__)
#define DBG_LOG(fmt, ...) LOG(DBG, fmt, ##__VA_ARGS__)
#define ERR_LOG(fmt, ...) LOG(ERR, fmt, ##__VA_ARGS__)
class Buffer
{
public:
    Buffer():_read_index(0),_write_index(0),_capacity(BUFFER_SIZE),_buffer(_capacity) {}
    ~Buffer() {}
    uint64_t Write(const void* src,uint64_t len)
    {
        EnsureMemEnough(len);
        const char* data = (const char*)src;
        std::copy(data,data+len,GetWritePos());
        //更新读写位置
        UpdateWritePos(len);
        return len;
    }
    uint64_t Read(void* dst,uint64_t len)
    {
        Peek(dst, len);
        UpdateReadPos(len);
        return len;
    }
    uint64_t Peek(void* dst,uint64_t len)
    {
        assert(len <= GetReadableSize());
        std::copy(GetReadPos(),GetReadPos() + len,(char*)dst);
        return len;
    }
    std::string PeekAsString(uint64_t len) 
    { 
        assert(len <= GetReadableSize());
        return std::string(GetReadPos(),len); 
    }
    uint64_t WriteAsString(const std::string& src) { return Write(src.c_str(),src.size()); }
    uint64_t WriteAsBuffer(Buffer& src) { return Write(src.GetReadPos(),src.GetReadableSize()); }
    std::string ReadAsString(uint64_t len) 
    { 
        assert(len <= GetReadableSize());
        std::string ret(GetReadPos(),len); 
        UpdateReadPos(len);
        return ret; 
    }

    bool Clear()
    {
        _read_index = 0;
        _write_index = 0;
        return true;
    }
    uint64_t GetReadableSize() { return _write_index - _read_index; }
    //这个是为了一行一行的提取HTTP协议内容
    std::string ParseByCRLF()
    {
        auto it = std::find(GetReadPos(),GetWritePos(),'\n');
        if(it != GetWritePos())
        {
            return std::string(GetReadPos(),it+1);//把\n也提取出来
        }
        else 
        {
            return "";     
        }
    }
private:

    //老师的项目中将char* 写成了void*，查询资料得知void*的加法是未定义的，g++编译器把其看成1，考虑到这些，我们用char*
    void UpdateWritePos(uint64_t len) { _write_index += len; }
    void UpdateReadPos(uint64_t  len) { _read_index += len; }
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
        if(GetTotalMem() >= len) 
        { 
            uint64_t readAbleLen = GetReadableSize();
            std::copy(GetReadPos(),GetWritePos(),GetBegin() );
            _read_index = 0;
            _write_index = readAbleLen;
            return true;
        }
        //进行合理的扩容和空间的挪移
        else 
        {
            uint64_t readAbleLen = GetReadableSize();
            _buffer.resize(len * 2 + _capacity,0); 
            _capacity = len * 2 + _capacity;
            std::copy(GetReadPos(),GetWritePos(),GetBegin());
            _read_index = 0;
            _write_index = readAbleLen;
            return true;
        }
    }

private:
    uint64_t _read_index;    
    uint64_t _write_index;
    uint64_t _capacity;
    std::vector<char> _buffer; 
};

