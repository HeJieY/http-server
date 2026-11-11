/*
 * @Author: HejieY 2145381080@qq.com
 * @Date: 2026-10-13 19:32:26
 * @FilePath: /muduo-http-server/source/Server.hpp
 * @Description: 这个类是高性能服务器的缓冲区,核心思想是使用vector<char>来存储数据,双指针维护空间
 * @Questions: 
 * 1.为什么类内设计时都是用地址而不用下标？：系统调用参数为地址，用地址方便与系统调用协调
 * 2.取地址返回的是指针,强转uint64_t就可以拿到具体的地址
 * 3.void* 加法是未定义问题,步长是不确定的
 * Copyright (c) 2026 by HeJieY, All Rights Reserved. 
 */
#pragma once
#include <memory>
#include<sys/epoll.h>
#include <asm-generic/socket.h>
#include <cerrno>
#include <cstddef>
#include<cstdio>
#include <functional>
#include<string>
#include<ctime>
#include<cstring>
#include<algorithm>
#include <cstdint>
#include <sys/types.h>
#include <unordered_map>
#include <utility>
#include<vector>
#include<cstdint>
#include<cassert>
#include<sys/socket.h>
#include<netinet/in.h>
#include<arpa/inet.h>
#include<unistd.h>
#include<fcntl.h>
#include<thread>
#include<sys/eventfd.h>
#include<mutex>
enum ExitCode
{
    EPOLLFD_CREATE_ERROR = 1,
    CREATE_SOCKET_ERROR,
    BIND_ERROR,
    LISTEN_SOCKET_CREATE_ERROR,
    EPOLL_CTL_ERROR,
    EPOLL_WAIT_ERROR,
    ACCEPT_ERROR,
    WRITE_ERROR,
    READ_ERROR,
};

#define BUFFER_SIZE 1024
#define INF 0
#define DBG 1
#define ERR 2
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

class EventLoop;    
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

class Socket
{
private:
    int _socketfd;
private:

public:
    Socket():_socketfd(-1) { }

    Socket(int socketfd):_socketfd(socketfd) { } 

    ~Socket() { }

    int GetSocket()
    {
        return _socketfd;
    }

    int CreateSocket() 
    {
        _socketfd = socket(AF_INET,SOCK_STREAM,0);
        if(_socketfd < 0) 
        {
            ERR_LOG("Socketfd create error!"); 
            exit(ExitCode::CREATE_SOCKET_ERROR);
        }
        return _socketfd;
    }

    int Bind(const std::string& ip,uint16_t port) 
    {
        struct sockaddr_in addr;
        memset(&addr,0,sizeof(addr));
        addr.sin_family = AF_INET;
        inet_pton(AF_INET, ip.c_str(), &addr.sin_addr);
        addr.sin_port  = htons(port);
        uint64_t len = sizeof(addr);
        int ret = bind(_socketfd,(struct sockaddr*)&addr,len);
        if(ret < 0) 
        { 
            ERR_LOG("Bind error!");
            perror("bind error");
        }
        return ret;
    }
 
    void SetReuseAddress(int op)  
    {
        setsockopt(_socketfd,SOL_SOCKET,SO_REUSEADDR,&op,sizeof(op));
    }

    bool CreateClientConnect(const std::string& ip,uint16_t  port)
    {   
        if(CreateSocket()  < 0) { exit(ExitCode::CREATE_SOCKET_ERROR); }
        sockaddr_in addr;
        memset(&addr, 0, sizeof(addr));
        inet_pton(AF_INET, ip.c_str(), &addr.sin_addr);
        addr.sin_port = htons(port);
        addr.sin_family = AF_INET;
        socklen_t len = sizeof(addr);
        int  ret = connect(_socketfd,(struct sockaddr*)&addr,len);
        if(ret < 0)
        {
           ERR_LOG("Connect error!");
           return false;
        }
        return true;
    }

    bool CreateServerConnect(std::string& ip,uint16_t port)
    {
        if(CreateSocket()  < 0) { exit(ExitCode::CREATE_SOCKET_ERROR); }
        //地址复用必须在bind之前
        SetReuseAddress(1);
        if(Bind(ip, port)<0) { exit(ExitCode::BIND_ERROR); }
        if(CreateListenConnect() == false) { exit(ExitCode::LISTEN_SOCKET_CREATE_ERROR); } 
        SetNonBlock();
        return true;
    }

    int ConnectToServer(const struct sockaddr* addr,socklen_t len)
    {
        int socketfd = connect(_socketfd,addr,len);
        if(socketfd < 0)
        {
            ERR_LOG("Connect error!");
        }
        return socketfd;
    }

    bool CreateListenConnect()
    {
        int ret = listen(_socketfd,8);
        if(ret < 0)
        {
            ERR_LOG("Listen socket create error!");
            return false;
        }
        return true;
    }

    //暂时调整为返回值为-1,-2,-3,之后在在进行其他调整
    int GetNewConnect()
    {
        struct sockaddr addr;
        socklen_t len = sizeof(addr); 
        memset(&addr,0,len);
        int ret = accept(_socketfd,&addr,&len);
        if(ret < 0)
        {
            if(errno == EAGAIN || errno == EWOULDBLOCK) { return -1;}
            else if(errno == EINTR) { return -2;}
            else 
            {  
                ERR_LOG("Accept error!"); 
                exit(ExitCode::ACCEPT_ERROR);
            }
        }
        else 
        {
            return ret;
        }        
    }

    bool Close()
    {
        close(_socketfd);
        return  true;
    }

    ssize_t Send(const char* buffer,size_t len,int flag = 0)
    {
        ssize_t n = send(_socketfd,buffer,len,flag);
        if(n < 0)
        {
            if(errno ==EAGAIN || errno == EWOULDBLOCK) { return -1; }
            else if(errno == EINTR) { return -2; }
            else 
            { 
                ERR_LOG("Send ");
                return -3;
            }
        }
        else
        {
            return n;
        }  
    }

    ssize_t SendAsNon(const char* buffer,size_t len)
    {
        return Send(buffer, len,MSG_DONTWAIT);
    }

    ssize_t Recv(char* buffer,int n,int flag = 0)
    {
        ssize_t sz = recv(_socketfd, buffer, n, flag);
        if(sz < 0)
        {
            if(errno ==EAGAIN || errno == EWOULDBLOCK) { return -1; }
            else if(errno == EINTR) { return -2; }
            else { return sz; }
        }
        else
        {
            return sz;
        }  
    }

    ssize_t RecvAsNon(char* buffer,int n)
    {
        return Recv(buffer,n,MSG_DONTWAIT);
    }

    int FdCntl(int op,int addFlag)
    {
        int flag = fcntl(_socketfd,F_GETFL,0);
        return fcntl(_socketfd, op, flag | addFlag); 
    }

    bool SetNonBlock()
    {
        FdCntl(F_SETFL,O_NONBLOCK);
        return true;
    }

};
class EventLoop;

class Channel
{
private:
    int _socketfd;
    EventLoop* _ptr;
    uint32_t _events; //表示的是某套接字要管理的事件
    uint32_t _revents;//表示的是某套接字哪些事件被触发了
    using EventCallback = std::function<void()>;
    EventCallback _read_callback;
    EventCallback _write_callback;
    EventCallback _error_callback;
    EventCallback _close_callback;
    EventCallback _event_callback;
public:
    Channel(int socketfd,EventLoop* ptr):_socketfd(socketfd),_ptr(ptr) { _events = 0; _revents = 0; }
    Channel() {}
    ~Channel() {}

    int GetFd() { return _socketfd; }
    uint32_t GetEvent() { return _events; }
    
    void SetEventCallback(EventCallback eventCallback);
    void SetWriteCallback(EventCallback writeCallback);
    void SetReadCallback(EventCallback readCallback) ;
    void SetCloseCallback(EventCallback closeCallback);
    void SetErrorCallback(EventCallback errorCallback) ;
    void CloseFd();

    void EnableRead();
    void EnableWrite();
    void DisableRead();
    void DisableWrite();
    void DisableAll();
    void RemoveEvent(uint32_t event);
    void SetRevents(uint32_t revents);
    void HandleEvent()
    {
        if((_revents & EPOLLIN) || (_revents & EPOLLRDHUP) || (_revents & EPOLLPRI))
        { if(_read_callback) _read_callback(); }
        if(_revents & EPOLLOUT)
        { if(_write_callback) _write_callback(); }
        if(_revents & EPOLLERR) 
        { if(_error_callback) _error_callback();}
        if(_revents & EPOLLHUP)
        { if(_close_callback) _close_callback(); }
    }
};


#define MAX_EVENTS 1024
class Poller
{
private:
    int _epollfd;
    std::unordered_map<int, Channel*> _map; //理论上遍历所有Channel也可以但是效率低
    struct epoll_event _events[MAX_EVENTS];
    int Update(Channel* channel,int op)
    {
        struct epoll_event event;
        event.data.fd = channel->GetFd();
        event.events = channel->GetEvent();
        INF_LOG("Add a event");
        return epoll_ctl(_epollfd,op,channel->GetFd(),&event);
    }
public:
    Poller()
    { 
        memset(_events, 0, sizeof(_events));
        _epollfd = epoll_create1(0);
        if(_epollfd < 0)
        {
            ERR_LOG("Epollfd create fail!");
            exit(ExitCode::EPOLLFD_CREATE_ERROR);
        }
        // std::cout << "Epoll init" << std::endl;
    }
    ~Poller() {}
    int UpdateEvent(Channel* channel) 
    {
        int ret;
        if(HaveChannel(channel) == false)
        {
            _map[channel->GetFd()] = channel;
            ret = Update(channel, EPOLL_CTL_ADD);
        }
        else 
        {
            ret = Update(channel, EPOLL_CTL_MOD);
        }
        if(ret < 0) 
        {
            ERR_LOG("Epoll control error!");
            perror("");
            exit(ExitCode::EPOLL_CTL_ERROR);
        }
        return ret;
    }
    bool HaveChannel(Channel* channel)
    {
        auto it = _map.find(channel->GetFd());
        if(it != _map.end()) return true;  
        return false;
    }
    void RemoveListen(Channel* channel)
    {
        // if(HaveChannel(channel) == false)
        //     _map[channel->GetFd()] = channel;
        int ret = Update(channel, EPOLL_CTL_DEL);
        if(ret < 0) 
        {
            ERR_LOG("Epoll control error!");
            exit(ExitCode::EPOLL_CTL_ERROR);
        }
    }
    int Poll(std::vector<Channel*>* active)
    {
        int ret = epoll_wait(_epollfd, _events, MAX_EVENTS, 10);
        if(ret == 0)
        {
            return 0;
        }
        else if(ret < 0)
        {
            if(errno ==EINTR) { return -1; }
            else { exit(ExitCode::EPOLL_WAIT_ERROR); }
        }
        else
        {
            for(int i=0;i<ret;i++)
            {
                
                auto it = _map.find(_events[i].data.fd);
                if(it != _map.end())
                {
                    
                    it->second->SetRevents(_events[i].events);
                    active->push_back(it->second);
                }
                else 
                {
                    ERR_LOG("No correct chaneel!");
                }
            }
            return ret;
        }
    }
    //TODO:这里没有释放fd，后面书写时记得补上
    bool RemoveChannel(int fd)
    {
        delete _map[fd];
        _map.erase(fd);
        return true;
    }
};

class EventLoop
{
private:
    using Task = std::function<void()>;

private:
    std::vector<Task> _tasks;
    int _eventfd;
    Channel _eventfd_channel;
    Poller _poller;
    std::mutex _mtx;
    std::thread::id _id;
    
    ssize_t EventFdCallBack()
    {
        uint64_t val;
        int ret = read(_eventfd,&val,sizeof(val));
        if(ret <= 0) 
        {
            if(errno == EAGAIN || errno == EINTR)
            {
                return 0;
            }
            else 
            {
                ERR_LOG("Read error");
                exit(ExitCode::READ_ERROR);
            }
        }
        return ret;
    }
public:
    EventLoop()
        :_id(std::this_thread::get_id())
        ,_eventfd(eventfd(0,EFD_NONBLOCK | EFD_CLOEXEC))
        ,_eventfd_channel(_eventfd,this)
    {        
        _eventfd_channel.EnableRead();
        _eventfd_channel.SetReadCallback(std::bind(&EventLoop::EventFdCallBack,this));
    }
    ~EventLoop()
    {
        
    }
    void UpdateChannel(Channel* channel);
    void RemoveChannel(Channel* channel);
    void WakeUpThread()
    {
        uint64_t val = 1;
        int ret = write(_eventfd,&val,sizeof(val));
        if(ret  < 0) 
        { 
            ERR_LOG("Write error!");
            exit(ExitCode::WRITE_ERROR);
        }
    }
    void RunInLoop(Task& task)
    {
       if(std::this_thread::get_id()  == _id)
       {
            task();
       }
       else 
       {
            QueueInLoop(task);
       }
    }
    void  QueueInLoop(Task&  task)
    {
        {
            std::lock_guard<std::mutex> mtx(_mtx);
            _tasks.push_back(task);
        }
        WakeUpThread();
    }
    void Loop()
    {
        while(true)
        {
            std::vector<Channel*> active;
            //1.先启动事件监控
            _poller.Poll(&active);
            //2.处理事件
            for(Channel* connect : active)
            {
                connect->HandleEvent();
            }
            //3.执行_tasks的任务，涉及线程安全
            //不循环处理_tasks这是为了线程不要长时间占有锁，影响效率
            std::vector<Task> tasks;//内部指向空间为nullptr但是实例是存在的可以使用
            {
                std::lock_guard<std::mutex> mtx(_mtx);//这个是RAII类型的出了作用域会自动释放锁
                std::swap(tasks,_tasks);//vector的swap是直接交换指针 
            }
                for(Task& t : tasks){ t(); }
            }
        }
    bool IsInThread(){ return _id == std::this_thread::get_id(); }  
};

    
inline void EventLoop::RemoveChannel(Channel* channel) { _poller.RemoveChannel(channel->GetFd()); }
inline void EventLoop::UpdateChannel(Channel* channel) { _poller.UpdateEvent(channel); }

inline void Channel::SetEventCallback(EventCallback eventCallback) { _event_callback = eventCallback; _ptr->UpdateChannel(this); }
inline void Channel::SetWriteCallback(EventCallback writeCallback) { _write_callback = writeCallback; _ptr->UpdateChannel(this); }
inline void Channel::SetReadCallback(EventCallback readCallback) { _read_callback = readCallback; _ptr->UpdateChannel(this);}
inline void Channel::SetCloseCallback(EventCallback closeCallback) { _close_callback = closeCallback; _ptr->UpdateChannel(this);}
inline void Channel::SetErrorCallback(EventCallback errorCallback) { _error_callback = errorCallback; _ptr->UpdateChannel(this);}
inline void Channel::EnableRead() { _events|= EPOLLIN ;_ptr->UpdateChannel(this); }
inline void Channel::EnableWrite() { _events |= EPOLLOUT; _ptr->UpdateChannel(this); }
inline void Channel::DisableRead() { _events &= ~EPOLLIN; _ptr->UpdateChannel(this);  }
inline void Channel::DisableWrite() { _events &= ~EPOLLOUT; _ptr->UpdateChannel(this); }
inline void Channel::DisableAll() { _events = 0; _ptr->UpdateChannel(this); }
inline void Channel::RemoveEvent(uint32_t event) { _events = event; _ptr->UpdateChannel(this); }
inline void Channel::SetRevents(uint32_t revents) { _revents = revents; _ptr->UpdateChannel(this); }
inline void Channel::CloseFd() { INF_LOG("Close  fd:%d",_socketfd); close(_socketfd); _ptr->RemoveChannel(this); }
