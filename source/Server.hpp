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
#include <sys/time.h>
#include<sys/timerfd.h>
#include <sys/epoll.h>
#include <asm-generic/socket.h>
#include <cerrno>
#include <cstddef>
#include <cstdio>
#include <functional>
#include <string>
#include <ctime>
#include <cstring>
#include <algorithm>
#include <cstdint>
#include <sys/types.h>
#include <unordered_map>
#include <utility>
#include <vector>
#include <cstdint>
#include <cassert>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <thread>
#include <sys/eventfd.h>
#include <mutex>
#include<time.h>
#include<memory>
static int id = 1; //TODO  这里采用全局变量肯定是不对的，但先测试把代码跑起来，后续完整了再处理这块
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
    RECV_ERROR,
    TIMERFD_CREATE_ERROR,
    NOT_FIND_TIMERTASK,
};

#define BUFFER_SIZE 1024
#define INF 0
#define DBG 1
#define ERR 2
#define LOG_LEVEL INF

#define LOG(level, fmt, ...)                                           \
    do                                                                 \
    {                                                                  \
        if (level >= LOG_LEVEL)                                        \
        {                                                              \
            time_t t = time(NULL);                                     \
            struct tm *tm_info = localtime(&t);                        \
            char time_buf[9];                                          \
            strftime(time_buf, sizeof(time_buf), "%H:%M:%S", tm_info); \
            fprintf(stdout, "[%s] %s:%d " fmt "\n",                    \
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
    Buffer() : _read_index(0), _write_index(0), _capacity(BUFFER_SIZE), _buffer(_capacity) {}
    ~Buffer() {}
    uint64_t Write(const void *src, uint64_t len)
    {
        EnsureMemEnough(len);
        const char *data = (const char *)src;
        std::copy(data, data + len, GetWritePos());
        // 更新读写位置
        UpdateWritePos(len);
        return len;
    }
    uint64_t Read(void *dst, uint64_t len)
    {
        Peek(dst, len);
        UpdateReadPos(len);
        return len;
    }
    uint64_t Peek(void *dst, uint64_t len)
    {
        assert(len <= GetReadableSize());
        std::copy(GetReadPos(), GetReadPos() + len, (char *)dst);
        return len;
    }
    std::string PeekAsString(uint64_t len)
    {
        assert(len <= GetReadableSize());
        return std::string(GetReadPos(), len);
    }
    uint64_t WriteAsString(const std::string &src) { return Write(src.c_str(), src.size()); }
    uint64_t WriteAsBuffer(Buffer &src) { return Write(src.GetReadPos(), src.GetReadableSize()); }
    std::string ReadAsString(uint64_t len)
    {
        assert(len <= GetReadableSize());
        std::string ret(GetReadPos(), len);
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
    // 这个是为了一行一行的提取HTTP协议内容
    std::string ParseByCRLF()
    {
        auto it = std::find(GetReadPos(), GetWritePos(), '\n');
        if (it != GetWritePos())
        {
            return std::string(GetReadPos(), it + 1); // 把\n也提取出来
        }
        else
        {
            return "";
        }
    }

private:
    // 老师的项目中将char* 写成了void*，查询资料得知void*的加法是未定义的，g++编译器把其看成1，考虑到这些，我们用char*
    void UpdateWritePos(uint64_t len) { _write_index += len; }
    void UpdateReadPos(uint64_t len) { _read_index += len; }
    char *GetBegin() { return &(*_buffer.begin()); }
    char *GetReadPos() { return GetBegin() + _read_index; }
    char *GetWritePos() { return GetBegin() + _write_index; }
    uint64_t GetWriteTailMem() { return _capacity - _write_index; }
    uint64_t GetTotalMem() { return GetWriteTailMem() + GetReadFrontMem(); }
    uint64_t GetReadFrontMem() { return _read_index; }
    bool EnsureMemEnough(uint64_t len)
    {
        if (len <= GetWriteTailMem())
        {
            return true;
        }
        // 进行空间的挪移
        if (GetTotalMem() >= len)
        {
            uint64_t readAbleLen = GetReadableSize();
            std::copy(GetReadPos(), GetWritePos(), GetBegin());
            _read_index = 0;
            _write_index = readAbleLen;
            return true;
        }
        // 进行合理的扩容和空间的挪移
        else
        {
            uint64_t readAbleLen = GetReadableSize();
            _buffer.resize(len * 2 + _capacity, 0);
            _capacity = len * 2 + _capacity;
            std::copy(GetReadPos(), GetWritePos(), GetBegin());
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
    Socket() : _socketfd(-1) {}

    Socket(int socketfd) : _socketfd(socketfd) {}

    ~Socket() {}

    int GetSocket()
    {
        return _socketfd;
    }

    int CreateSocket()
    {
        _socketfd = socket(AF_INET, SOCK_STREAM, 0);
        if (_socketfd < 0)
        {
            ERR_LOG("Socketfd create error!");
            exit(ExitCode::CREATE_SOCKET_ERROR);
        }
        return _socketfd;
    }
    // Return: <0 is error
    int Bind(const std::string &ip, uint16_t port)
    {
        struct sockaddr_in addr;
        memset(&addr, 0, sizeof(addr));
        addr.sin_family = AF_INET;
        inet_pton(AF_INET, ip.c_str(), &addr.sin_addr);
        addr.sin_port = htons(port);
        uint64_t len = sizeof(addr);
        int ret = bind(_socketfd, (struct sockaddr *)&addr, len);
        return ret;
    }

    void SetReuseAddress(int op)
    {
        setsockopt(_socketfd, SOL_SOCKET, SO_REUSEADDR, &op, sizeof(op));
    }

    bool CreateClientConnect(const std::string &ip, uint16_t port)
    {
        if (CreateSocket() < 0)
        {
            exit(ExitCode::CREATE_SOCKET_ERROR);
        }
        sockaddr_in addr;
        memset(&addr, 0, sizeof(addr));
        inet_pton(AF_INET, ip.c_str(), &addr.sin_addr);
        addr.sin_port = htons(port);
        addr.sin_family = AF_INET;
        socklen_t len = sizeof(addr);
        int ret = connect(_socketfd, (struct sockaddr *)&addr, len);
        if (ret < 0)
        {
            ERR_LOG("Connect error!");
            return false;
        }
        return true;
    }

    bool CreateServerConnect(std::string &ip, uint16_t port)
    {
        if (CreateSocket() < 0)
        {
            exit(ExitCode::CREATE_SOCKET_ERROR);
        }
        // 地址复用必须在bind之前
        SetReuseAddress(1);
        if (Bind(ip, port) < 0)
        {
            exit(ExitCode::BIND_ERROR);
        }
        if (CreateListenConnect() < 0)
        {
            exit(ExitCode::LISTEN_SOCKET_CREATE_ERROR);
        }
        SetNonBlock();
        return true;
    }

    int ConnectToServer(const struct sockaddr *addr, socklen_t len)
    {
        int socketfd = connect(_socketfd, addr, len);
        if (socketfd < 0)
        {
            ERR_LOG("Connect error!");
        }
        return socketfd;
    }

    int CreateListenConnect()
    {
        return listen(_socketfd, 8);
    }

    // 暂时调整为返回值为-1,-2,-3,之后在在进行其他调整
    int GetNewConnect()
    {
        struct sockaddr addr;
        socklen_t len = sizeof(addr);
        memset(&addr, 0, len);
        int ret = accept(_socketfd, &addr, &len);
        if (ret < 0)
        {
            if (errno == EAGAIN || errno == EWOULDBLOCK)
            {
                return -1;
            }
            else if (errno == EINTR)
            {
                return -2;
            }
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
        return true;
    }

    ssize_t Send(const char *buffer, size_t len, int flag = 0)
    {
        ssize_t n = send(_socketfd, buffer, len, flag);
        if (n < 0)
        {
            if (errno == EAGAIN || errno == EWOULDBLOCK)
            {
                return -1;
            }
            else if (errno == EINTR)
            {
                return -2;
            }
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

    ssize_t SendAsNon(const char *buffer, size_t len)
    {
        return Send(buffer, len, MSG_DONTWAIT);
    }

    ssize_t Recv(char *buffer, int n, int flag = 0)
    {
        ssize_t sz = recv(_socketfd, buffer, n, flag);
        if (sz < 0)
        {
            if (errno == EAGAIN || errno == EWOULDBLOCK)
            {
                return -1;
            }
            else if (errno == EINTR)
            {
                return -2;
            }
            else
            {
                return sz;
            }
        }
        else
        {
            return sz;
        }
    }

    ssize_t RecvAsNon(char *buffer, int n)
    {
        return Recv(buffer, n, MSG_DONTWAIT);
    }

    int FdCntl(int op, int addFlag)
    {
        int flag = fcntl(_socketfd, F_GETFL, 0);
        return fcntl(_socketfd, op, flag | addFlag);
    }

    bool SetNonBlock()
    {
        FdCntl(F_SETFL, O_NONBLOCK);
        return true;
    }
};
class EventLoop;

class Channel
{
private:
    
    int _socketfd;
    EventLoop *_ptr;
    uint32_t _events;  // 表示的是某套接字要管理的事件
    uint32_t _revents; // 表示的是某套接字哪些事件被触发了
    using EventCallback = std::function<void()>;
    EventCallback _read_callback;
    EventCallback _write_callback;
    EventCallback _error_callback;
    EventCallback _close_callback;
    EventCallback _event_callback;

public:
    int _id;// 临时添加的id用于测试TimeWheel
    Channel(int socketfd, EventLoop *ptr) : _socketfd(socketfd), _ptr(ptr)
    {
        _events = 0;
        _revents = 0;
    }
    Channel() {}
    ~Channel() {}

    int GetFd() { return _socketfd; }
    uint32_t GetEvent() { return _events; }
    void Update();
    void Remove();
    //设置回调函数
    void SetEventCallback(EventCallback eventCallback) { _event_callback = eventCallback; }
    void SetWriteCallback(EventCallback writeCallback) { _write_callback = writeCallback; }
    void SetReadCallback(EventCallback readCallback)   { _read_callback = readCallback;   }
    void SetCloseCallback(EventCallback closeCallback) { _close_callback = closeCallback; }
    void SetErrorCallback(EventCallback errorCallback) { _error_callback = errorCallback; }
    void EnableRead()
    {
        _events |= EPOLLIN;
        Update();
    }
    void EnableWrite()
    {
        _events |= EPOLLOUT;
        Update();
    }
    void DisableRead()
    {
        _events &= ~EPOLLIN;
        Update();
    }
    void DisableWrite()
    {
        _events &= ~EPOLLOUT;
        Update();
    }
    void DisableAll()
    {
        _events = 0;
        Update();
    }
    //移除某事件
    void RemoveEvent(uint32_t event) 
    { 
        _events &= ~event;
        Update();
    }
    void SetRevents(uint32_t revents)
    {
        _revents = revents;
    }
    void CloseFd() 
    { 
        //如果一次事件里 HandleRead 已经关闭了连接，后面 EPOLLHUP 又调用 HandleClose，就会第二次 CloseFd

        if (_socketfd < 0) return; 
        Remove();
        close(_socketfd);
        INF_LOG("Close  fd:%d", _socketfd); 
        _socketfd = -1;
    }    
    //临时添加的成员函数
    void SetId(uint64_t id)
    {
        _id = id;
    }
    
    void HandleEvent()
    {
        // 关于EPOLLHUP和EPOLLREDHUP
        // EPOLLHUP：指的是对方fd已不可使用，因为各种原因未收到FIN便断开，需要将该链接close
        // EPOLLRDHUP：指收到对方FIN，我方仍可发送数据，处理完业务后close
        if ((_revents & EPOLLIN) || (_revents & EPOLLRDHUP) || (_revents & EPOLLPRI))
        {
            if (_read_callback) _read_callback();
        }
        if (_revents & EPOLLOUT)
        {
            if (_write_callback) _write_callback();
        }
        if (_revents & EPOLLERR)
        {
            if (_error_callback) _error_callback();
        }
        if (_revents & EPOLLHUP)
        {
            if (_close_callback) _close_callback();
        }
        if(_event_callback) _event_callback();
    }
};

#define MAX_EVENTS 1024
class Poller
{
private:
    int _epollfd;
    std::unordered_map<int, Channel *> _map; // 理论上遍历所有Channel也可以但是效率低
    struct epoll_event _events[MAX_EVENTS];
    int Update(Channel *channel, int op)
    {
        struct epoll_event event;
        event.data.fd = channel->GetFd();
        event.events = channel->GetEvent();

        INF_LOG("Add a event");
        return epoll_ctl(_epollfd, op, channel->GetFd(), &event);
    }

public:
    Poller()
    {
        memset(_events, 0, sizeof(_events));
        _epollfd = epoll_create1(0);
        if (_epollfd < 0)
        {
            ERR_LOG("Epollfd create fail!");
            exit(ExitCode::EPOLLFD_CREATE_ERROR);
        }
        // std::cout << "Epoll init" << std::endl;
    }
    ~Poller() {}
    int UpdateEvent(Channel *channel)
    {
        int ret;
        if (HaveChannel(channel) == false)
        {
            _map[channel->GetFd()] = channel;
            ret = Update(channel, EPOLL_CTL_ADD);
        }
        else
        {
            ret = Update(channel, EPOLL_CTL_MOD);
        }
        if (ret < 0)
        {
            ERR_LOG("Epoll control error!");
            perror("");
            exit(ExitCode::EPOLL_CTL_ERROR);
        }
        return ret;
    }
    bool HaveChannel(Channel *channel)
    {
        auto it = _map.find(channel->GetFd());
        if (it != _map.end())
            return true;
        return false;
    }
    void RemoveListen(Channel *channel)
    {
        // if(HaveChannel(channel) == false)
        //     _map[channel->GetFd()] = channel;
        int ret = Update(channel, EPOLL_CTL_DEL);
        if (ret < 0)
        {
            ERR_LOG("Epoll control error!");
            exit(ExitCode::EPOLL_CTL_ERROR);
        }
        RemoveMapDate(channel->GetFd());
    }
    int Poll(std::vector<Channel *> *active)
    {
        int ret = epoll_wait(_epollfd, _events, MAX_EVENTS, 10);
        if (ret == 0)
        {
            return 0;
        }
        else if (ret < 0)
        {
            if (errno == EINTR)
            {
                return -1;
            }
            else
            {
                exit(ExitCode::EPOLL_WAIT_ERROR);
            }
        }
        else
        {
            for (int i = 0; i < ret; i++)
            {

                auto it = _map.find(_events[i].data.fd);
                if (it != _map.end())
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
    // TODO:这里没有释放fd，后面书写时记得补上
    bool RemoveMapDate(int fd)
    {
        // delete _map[fd];
        //这里先不能删除因为这里是又channel发起的close，暂时先不处理
        _map.erase(fd);
        return true;
    }
};
class TimeWheel;
class Test;
using To_Death_Cb = std::function<void()>;
using ReleaseInfo = std::function<bool()>;

class TimeTask
{
private:
    
    uint64_t _id;           //用于唯一标识任务
    uint32_t _timeout;      //用于标识任务的最大存活时间
    uint32_t _delay;        //用于表示每次应当延长寿命多久
    uint32_t _dead_index;     //用于表示任务将在哪一秒死亡
    To_Death_Cb _death_task;//用于表示时间到后自动执行的任务 
    ReleaseInfo _release;   //这个是回调用于删除map中的信息避免内存泄漏
  
public:
    TimeTask(const uint64_t id,const uint32_t timeout,uint32_t delay,const uint32_t dead_index ,const To_Death_Cb& death_task)
        :_id(id),_timeout(timeout),_death_task(death_task),_delay(delay),_dead_index(dead_index) {}
    ~TimeTask()
    {
        _death_task();
        if(_release)
        _release();
    }
    bool SetRelease(ReleaseInfo release)
    {
        _release = release;
        return true;
    }
    uint32_t GetTimeOut() { return _timeout; } const
    uint32_t GetStdDelay() { return _delay; } const
    uint32_t GetDeadIndex() { return _dead_index; } const
    void SetDeadTick(uint32_t dead_index) { _dead_index = dead_index; }
};

class EventLoop;
class TimeWheel
{
    //C++ 成员按照声明顺序初始化调用构造,按照声明顺序的逆序调用析构
    using STaskPtr = std::shared_ptr<TimeTask>;
    using WTaskPtr = std::weak_ptr<TimeTask>;
private:
    std::unordered_map<uint64_t,WTaskPtr> _map; //表示id与任务之间的联系
    std::vector<std::vector<STaskPtr>> _tasks;  //表示所有待办任务
    uint64_t _capacity;                         //表示时间轮的最大容量
    uint32_t _ticks;                            //表示秒针
    EventLoop* _ptr;                            //EventLoop* 回指指针
    int _timerfd;
    std::shared_ptr<Channel> _timer_channel;

private:

    bool RemoveTask(uint64_t id)
    {
       auto it = _map.find(id);
       if(it != _map.end())
       {
           _map.erase(it);
            return true;
       }
       {
           return false;
       }
    }

   void AddTaskInLoop(const uint32_t id,uint32_t timeout,uint32_t delay,const To_Death_Cb& cb)
    {
        //找到对应位置,添加要执行的任务去tasks中
        uint64_t pos = (_ticks + timeout) % _capacity;
        STaskPtr ptr = std::make_shared<TimeTask>(id,timeout,delay,pos,cb);
        ptr->SetRelease(std::bind(&TimeWheel::RemoveTask,this,id));
        //这么写是不对的c++标准规定:非静态成员函数(不含this的)函数名不会自动强转成函数指针
        // ptr->SetRelease(std::bind(RemoveTask,this,id));
        _tasks[pos].push_back(ptr);
        // 添加到_map中加以管理
        _map[id] = WTaskPtr(ptr);
    }
    void ReflashInLoop(uint64_t id)
    {
        auto it = _map.find(id);  
        if(it != _map.end())
        {
            //计算位置,并添加进入相应位置
            //这是存在问题的，原因在于从weak指针中拿取得原对象，新创建的
            //share_ptr不会共享引用计数
            //_tasks[pos].push_back(STaskPtr(it->second.lock()));            
            STaskPtr ptr = it->second.lock();
            uint32_t pos = (ptr->GetDeadIndex() + ptr->GetStdDelay()) % _capacity;
            ptr->SetDeadTick(pos);
            _tasks[pos].push_back(ptr); 
        }
        else
        {
            //不应该出现的错误
            ERR_LOG("Not find timertaks");
            exit(ExitCode::NOT_FIND_TIMERTASK);
       } 
    }

    void CreateTimerFdInLoop(uint32_t sec,uint32_t cycle)
    {
         //进程调用 exec() 启动新程序时，内核自动关闭该 fd，防止 fd 泄漏给子进程。
        struct itimerspec lti;
        memset(&lti, 0, sizeof(struct itimerspec));
        //这个表示下个重复周期，成员表示纳秒和秒
        lti.it_interval.tv_sec = cycle;//
        lti.it_interval.tv_nsec = 0;
        lti.it_value.tv_nsec = 0;
        lti.it_value.tv_sec = sec;
        timerfd_settime(_timerfd, 0, &lti, NULL);
    }
public:
    TimeWheel(uint64_t  capacity,EventLoop* ptr)
        :_capacity(capacity)
        ,_ticks(0)
        ,_tasks(capacity)
        ,_ptr(ptr)
        ,_timerfd(_timerfd = timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK | TFD_CLOEXEC))
        ,_timer_channel(std::make_shared<Channel>(_timerfd,_ptr))
        {
            _timer_channel->EnableRead();
            _timer_channel->SetReadCallback(std::bind(&TimeWheel::OnTime,this));
            if(_timerfd < 0)
            {
                ERR_LOG("Timerfd create error!");
                exit(ExitCode::TIMERFD_CREATE_ERROR);
            }
            
        }
    ~TimeWheel() 
    { 
        
    }

    //  关于TFD_CLOEXEC:子进程会对父进程的虚拟地址空间进行继承（父子进程同样的页表），只有当对父进程的数据发生操作的时候会发生
    //写时拷贝，修改页表指向其他物理内存地址，子进程退出时会清理进程资源，会保存的时进程退出码（保存在PCB之中task_struct），需要等父进
    //程回收如果子进程继承了fd，文件描述符的涉及是引用计数，fork时其引用计数会+1，子进程不知道该fd的作用可能就不释放，导致fd凭空+1，后续父进程也关不掉了
    //时间到了执行任务
    void OnTime()
    {
        ReadTimer();
        TickMove();
    }

    //sec  : 启动后多少秒之后生效
    //cycle: 再此之后的执行周期为多少秒
    void CreateTimerFd(uint32_t sec,uint32_t cycle);


    //读取timerfd的内容
    void ReadTimer()
    {
        while(1)
        {
            uint64_t exp = 0;
            ssize_t ret = read(_timerfd,&exp,sizeof(uint64_t));
            if(ret == sizeof(uint64_t))
            {
                //执行任务
                return;
            }
            else if(ret < 0 && errno == EINTR)
            {
                //信号中断可重试
                continue;
            }
            else if (ret < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) return; 
            //发生不应该发生的错误，直接退出即可
            ERR_LOG("Read error!"); 
            exit(ExitCode::READ_ERROR);

        }
    }

    //刷新事件的生存时间
    void Reflash(uint64_t id);
    
    //添加一个任务
    void AddTask(const uint64_t id,const uint32_t timeout,uint32_t delay,const To_Death_Cb& cb);

    //tick增加1s,并且处理到时间了的任务
    void TickMove()
    {
        _tasks[_ticks].clear();
        _ticks = (_ticks+1) % _capacity;
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
    TimeWheel _time_wheel;

    ssize_t EventFdCallBack()
    {
        uint64_t val;
        int ret = read(_eventfd, &val, sizeof(val));
        if (ret <= 0)
        {
            if (errno == EAGAIN || errno == EINTR)
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
    EventLoop(uint64_t timerWheelCapacity)
        : _id(std::this_thread::get_id())
        , _eventfd(eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC))
        , _eventfd_channel(_eventfd, this)
        ,_time_wheel(timerWheelCapacity, this)
    {
        _eventfd_channel.EnableRead();
        _eventfd_channel.SetReadCallback(std::bind(&EventLoop::EventFdCallBack, this));
    }
    ~EventLoop()
    {
    }
    void UpdateChannel(Channel *channel) { _poller.UpdateEvent(channel);  }
    void RemoveChannel(Channel *channel) { _poller.RemoveListen(channel); }
    void WakeUpThread()
    {
        uint64_t val = 1;
        int ret = write(_eventfd, &val, sizeof(val));
        if (ret < 0)
        {
            ERR_LOG("Write error!");
            exit(ExitCode::WRITE_ERROR);
        }
    }
    void RunInLoop(const Task &task)
    {
        if (std::this_thread::get_id() == _id)
        {
            task();
        }
        else
        {
            QueueInLoop(task);
        }
    }
    void QueueInLoop(const Task &task)
    {
        {
            std::lock_guard<std::mutex> mtx(_mtx);
            _tasks.push_back(task);
        }
        WakeUpThread();
    }
    void RunAllTasks()
    {
        std::vector<Task> tasks; // 内部指向空间为nullptr但是实例是存在的可以使用
        {
            std::lock_guard<std::mutex> mtx(_mtx); // 这个是RAII类型的出了作用域会自动释放锁
            std::swap(tasks, _tasks);              // vector的swap是直接交换指针
        }
        for (Task &t : tasks)
        {
            t();
        }
    }
    void Loop()
    {
        while (true)
        {
            std::vector<Channel *> active;
            // 1.先启动事件监控
            _poller.Poll(&active);
            // 2.处理事件
            for (Channel *connect : active)
            {
                connect->HandleEvent();
            }
            // 3.执行_tasks的任务，涉及线程安全
            // 不循环处理_tasks这是为了线程不要长时间占有锁，影响效率
            RunAllTasks();
        }
    }
    bool IsInThread() { return _id == std::this_thread::get_id(); }

    //时间轮相关

    void AddTimerTask(uint32_t id,const uint32_t timeout,uint32_t delay,const Task& cb)
    {
        _time_wheel.AddTask(id, timeout,delay, cb);   
    }
    void ReflashTimer(uint64_t id)
    {
        _time_wheel.Reflash(id);
    }
    void CreateTimerFd(uint32_t sec, uint32_t cycle)
    {
        _time_wheel.CreateTimerFd(sec,cycle);
    }
};

inline void Channel::Update() { _ptr->UpdateChannel(this); }
inline void Channel::Remove() { _ptr->RemoveChannel(this); }


inline void TimeWheel::AddTask(const uint64_t id,const uint32_t timeout,uint32_t delay,const To_Death_Cb& cb) 
{
    _ptr->RunInLoop(std::bind(&TimeWheel::AddTaskInLoop,this,id,timeout,delay,cb));
}
inline void TimeWheel::Reflash(uint64_t id) 
{
    _ptr->RunInLoop(std::bind(&TimeWheel::ReflashInLoop,this,id));
}
inline void TimeWheel::CreateTimerFd(uint32_t sec,uint32_t cycle)
{
    _ptr->RunInLoop(std::bind(&TimeWheel::CreateTimerFdInLoop,this,sec,cycle)); 
}