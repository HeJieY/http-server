/*
 * @Author: HejieY 2145381080@qq.com
 * @Date: 2026-10-19 14:36:19
 * @FilePath: /muduo-http-server/source/Main.cc
 * @Description: 
 * Copyright (c) 2026 by HeJieY, All Rights Reserved. 
 */
#include "Server.hpp"

int  Acceptor(int lisfd)
{
    return accept(lisfd,nullptr,nullptr);
}

void HandleRead()
{

}
void HandleWrite()
{

}
void HandleClose()
{

}
void HandleError()
{

}
void HandleEvent()
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
    channel->SetReadCallback();
    channel->SetWriteCallback();
    channel->SetCloseCallback();
    channel->SetErrorCallback();
    channel->SetEventCallback();
    poller.UpdateEvent(channel);    
}


int main()
{
    
    Socket lisSocket;
    Poller poller;
    Channel lisChannel(lisSocket.GetSocket());
    std::vector<Channel*> active;
    lisSocket.CreateSocket();
    lisSocket.Bind("118.89.172.104",8081);
    lisSocket.CreateListenConnect();
    

    lisChannel.EnableRead();
    lisChannel.SetReadCallback(std::bind(HandleListenRead,lisSocket,poller));

    poller.UpdateEvent(&lisChannel);
    poller.Poll(active);

    for(Channel* channel  : active)
    {
        channel->HandleEvent(); 
    }
}
