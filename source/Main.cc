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
#include <functional>
#include <sys/socket.h>
#include <unistd.h>

int  Acceptor(int lisfd)
{
    return accept(lisfd,nullptr,nullptr);
}

void HandleRead(Channel* channel)
{
    std::string buffer;
    buffer.resize(1024);
    recv(channel->GetFd(), &buffer[0], BUFFER_SIZE,MSG_DONTWAIT);
    std::cout << "Client say# "<< buffer.c_str()  << std::endl;
}
void HandleWrite(Channel* channel)
{

}
void HandleClose(Channel* channel,Poller* poller)
{
    poller->RemoveChannel(channel->GetFd());
}
void HandleError(Channel* channel)
{

}
void HandleEvent(Channel* channel)
{
    
}
void HandleListenRead(Socket& lisSocket,Poller& poller)
{
    int fd =  Acceptor(lisSocket.GetSocket());
    if(fd  < 0)
    {
        ERR_LOG("Accept error!");
        exit(ExitCode::ACCEPT_ERROR);
    }
    
    //创建Channel，添加事件管理，注册到Poller
    Channel* channel = new Channel(fd);
    channel->EnableRead();
    channel->EnableWrite();
    channel->SetReadCallback(std::bind(HandleRead,channel));
    channel->SetWriteCallback(std::bind(HandleWrite,channel));
    channel->SetCloseCallback(std::bind(HandleClose,channel,&poller));
    channel->SetErrorCallback(std::bind(HandleError,channel));
    channel->SetEventCallback(std::bind(HandleEvent,channel));
    poller.UpdateEvent(channel);    
}


int main()
{
    
    Socket lisSocket;
    Poller poller;
    Channel* lisChannel =  new Channel(lisSocket.GetSocket());
    std::vector<Channel*> active;
    lisSocket.CreateSocket();
    lisSocket.Bind("118.89.172.104",8081);
    lisSocket.CreateListenConnect();
    

    lisChannel->EnableRead();
    lisChannel->SetReadCallback(std::bind(HandleListenRead,lisSocket,poller));

    poller.UpdateEvent(lisChannel);
    poller.Poll(active);

    for(Channel* channel  : active)
    {
        channel->HandleEvent(); 
    }
}
