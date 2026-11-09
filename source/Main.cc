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
#include <cstring>
#include <functional>
#include <sys/socket.h>
#include <unistd.h>
#include <vector>

int  Acceptor(int lisfd)
{
    return accept(lisfd,nullptr,nullptr);
}

void HandleRead(Channel* channel)
{
    std::string buffer;
    buffer.resize(1024);
    int ret = recv(channel->GetFd(), &buffer[0], BUFFER_SIZE,MSG_DONTWAIT);
    if(ret < 0)
    {
           
    }
    else 
    {   
        std::cout << "Client say# "<< buffer.c_str()  << std::endl;
    }
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
void HandleListenRead(Socket* lisSocket,Poller* poller)
{
    int fd =  Acceptor(lisSocket->GetSocket());
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
    channel->SetCloseCallback(std::bind(HandleClose,channel,poller));
    channel->SetErrorCallback(std::bind(HandleError,channel));
    channel->SetEventCallback(std::bind(HandleEvent,channel));
    poller->UpdateEvent(channel);    
}


int main()
{
    
    Socket* lisSocket = new Socket();
    Poller* poller = new Poller();
    std::vector<Channel*>* active = new std::vector<Channel*>();
    active->reserve(MAX_EVENTS);
    lisSocket->CreateSocket();
    lisSocket->Bind("0.0.0.0",8081); 
    lisSocket->CreateListenConnect();
    Channel* lisChannel =  new Channel(lisSocket->GetSocket());

    lisChannel->EnableRead();
    //bind函数里面的参数是对上文变量的拷贝，回调函数是对该拷贝的引用原文变量没有发生变化
    lisChannel->SetReadCallback(std::bind(HandleListenRead,lisSocket,poller));
    int ret = poller->UpdateEvent(lisChannel);
    if(ret < 0)
    {
        ERR_LOG("Update event fail");
    }
    while(1)
    {
        //记得清除任务
        active->clear();
        int ret = poller->Poll(active);
        if(ret == 0) 
        { 
            INF_LOG("No event happen!"); 
        }
        else if(ret == -1)
        {
            INF_LOG("The call was interrupted by a singal!");
        }
        else
        {
            
            for(Channel* channel : *active)
            {
                channel->HandleEvent(); 
            }
        }
        sleep(1);
    }

    
}
