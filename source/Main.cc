/*
 * @Author: HejieY 2145381080@qq.com
 * @Date: 2026-10-27 17:03:28
 * @FilePath: /muduo-http-server/source/Main.cc
 * @Description:
 * Copyright (c) 2026 by HeJieY, All Rights Reserved.
 */
/*
 * @Author: HejieY 2145381080@qq.com
 * @Date: 2026-10-19 14:36:19
 * @FilePath: /muduo-http-server/source/Main.cc
 * @Description:
 * Copyright (c) 2026 by HeJieY, All Rights Reserved.
 */
#include "Server.hpp"
#include <cerrno>
#include <cstring>
#include <functional>
#include <sys/socket.h>
#include <unistd.h>

int Acceptor(int lisfd)
{
    return accept(lisfd, nullptr, nullptr);
}

void HandleRead(Channel *channel)
{
    //resize ：改变的是实际大小，改不改变capacity取决于resize与capacity的大小情况，resize是扩充到多大不是加多大的概念，resize对于新添加的元素会填充初始值0
    //reserve：改变的是capacity，默认不会初始化内容
    //cout   ：打印的是string的size大小
    char buffer[1024] = {0};
    while (1)
    {
        int ret = recv(channel->GetFd(),&buffer[0],BUFFER_SIZE,MSG_DONTWAIT);
        if(ret == 0)
        {
            //对端正常关闭,理论上关闭该连接，在发送完我要发送的数据之后：某端关闭连接后处于TIME_WAIT状态，可以进行数据发送，直到对端close，收到ack确认报文
            //处理应该是要把待处理的业务全部发送，但这里还没接触到所以暂时直接close
            channel->CloseFd();
            return;
        }
        else if(ret < 0)
        {
            //这里表示recv出错了但有几个情况要处理
            if(errno == EAGAIN || errno == EWOULDBLOCK)
            {
                //数据读完了，可以结束循环
                return;
            }
            else if(errno == EINTR)
            {
                //收到信号被中断，仍然可以执行
                continue;
            }
            else
            {
                ERR_LOG("Recv error");
                exit(ExitCode::RECV_ERROR);
            }
        }
        else
        {
            //这么处理也不对你怎么知道读完了？？应该先把数据存起来，完全读完之后或者记录下i下次从i开始读，但是先不改了，等后面引入缓冲区再解决吧
            fwrite(buffer, 1, ret, stdout);
            fflush(stdout);
        }
       
    }
    //这里应该将buffer通过回调交给上层函数进行业务处理了暂时还没设计，所以先简单打印
    //buffer.resize(total);
    //std::cout << "client say:" << std::endl; 
    //***这么写也不对，因为resize会把新的部分初始化为0，虽然你写了，但它不知道

  
    printf("\n");
}
void HandleWrite(Channel *channel)
{
    //这个暂时先不处理，目前没有这个状况
}

void HandleClose(Channel *channel)
{
    channel->CloseFd();
}
void HandleError(Channel *channel)
{
    //错误处理具体到后面业务再说
}
void HandleEvent(Channel *channel, EventLoop *loop)
{
    loop->ReflashTimer(channel->_id);
}
void HandleListenRead(Socket *lisSocket, EventLoop *loop)
{
    int fd = Acceptor(lisSocket->GetSocket());
    if (fd < 0)
    {
        ERR_LOG("Accept error!");
        exit(ExitCode::ACCEPT_ERROR);
    }
    //添加到TimerWheel中用于管理连接生命周期
    //TODO:如果在这里为他绑定了关闭的函数，那么当epoll接受到关闭信号之后，epoll调用HandleClose可能就释放了，应该怎么办呢？,估计和后面连接模块有关暂时不做处理先跑起来
    loop->AddTimerTask(id,5,5,std::bind([fd](){
        INF_LOG("Close fd:%d",fd);
        close(fd);
    }));
    // 创建Channel，添加事件管理，注册到Poller
    Channel *channel = new Channel(fd, loop);
    channel->EnableRead();
    //channel->EnableWrite();   会一直导致一直空转
    channel->SetReadCallback(std::bind(HandleRead, channel));
    channel->SetWriteCallback(std::bind(HandleWrite, channel));
    channel->SetCloseCallback(std::bind(HandleClose, channel));
    channel->SetErrorCallback(std::bind(HandleError, channel));
    channel->SetEventCallback(std::bind(HandleEvent,channel,loop));
    channel->SetId(id);
    id++;
}

int main()
{

    Socket *lisSocket = new Socket();
    EventLoop *loop = new EventLoop(60);
    loop->CreateTimerFd(1, 1);
    lisSocket->CreateSocket();
    lisSocket->SetReuseAddress(1);
    if (lisSocket->Bind("0.0.0.0", 8081) < 0)
    {
        ERR_LOG("Bind error");
        exit(ExitCode::BIND_ERROR);
    }

    if (lisSocket->CreateListenConnect() < 0)
    {
        ERR_LOG("Create listen socket error!");
        exit(ExitCode::CREATE_SOCKET_ERROR);
    }

    Channel *lisChannel = new Channel(lisSocket->GetSocket(), loop);

    lisChannel->EnableRead();
    // bind函数里面的参数是对上文变量的拷贝，回调函数是对该拷贝的引用原文变量没有发生变化
    lisChannel->SetReadCallback(std::bind(HandleListenRead, lisSocket, loop));

    loop->Loop();
}
