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
#include<iostream>

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
void HandleClose(Channel* channel)
{
    
}
void HandleError(Channel* channel)
{

}
void HandleEvent(Channel* channel)
{
    
}
void HandleListenRead(Socket* lisSocket,EventLoop* loop)
{
    int fd =  Acceptor(lisSocket->GetSocket());
    if(fd  < 0)
    {
        ERR_LOG("Accept error!");
        exit(ExitCode::ACCEPT_ERROR);
    }
    
    //创建Channel，添加事件管理，注册到Poller
    Channel* channel = new Channel(fd,loop);
    channel->EnableRead();
    channel->EnableWrite();
    channel->SetReadCallback(std::bind(HandleRead,channel));
    channel->SetWriteCallback(std::bind(HandleWrite,channel));
    channel->SetCloseCallback(std::bind(HandleClose,channel));
    channel->SetErrorCallback(std::bind(HandleError,channel));
    channel->SetEventCallback(std::bind(HandleEvent,channel));
    
}


int main()
{
    
    Socket* lisSocket = new Socket();
    EventLoop* loop = new EventLoop();
    
    std::vector<Channel*>* active = new std::vector<Channel*>();
    active->reserve(MAX_EVENTS);
    lisSocket->CreateSocket();
    lisSocket->Bind("0.0.0.0",8081); 
    lisSocket->CreateListenConnect();
    Channel* lisChannel =  new Channel(lisSocket->GetSocket(),loop);

    lisChannel->EnableRead();
    //bind函数里面的参数是对上文变量的拷贝，回调函数是对该拷贝的引用原文变量没有发生变化
    lisChannel->SetReadCallback(std::bind(HandleListenRead,lisSocket,loop));
    
    loop->Loop();
}
void EventLoop::RemoveChannel(Channel* channel) { _poller.RemoveChannel(channel->GetFd()); }
void EventLoop::UpdateChannel(Channel* channel) { _poller.UpdateEvent(channel); }

void Channel::SetEventCallback(EventCallback eventCallback) { _event_callback = eventCallback; _ptr->UpdateChannel(this); }
void Channel::SetWriteCallback(EventCallback writeCallback) { _write_callback = writeCallback; _ptr->UpdateChannel(this); }
void Channel::SetReadCallback(EventCallback readCallback) { _read_callback = readCallback; _ptr->UpdateChannel(this);}
void Channel::SetCloseCallback(EventCallback closeCallback) { _close_callback = closeCallback; _ptr->UpdateChannel(this);}
void Channel::SetErrorCallback(EventCallback errorCallback) { _error_callback = errorCallback; _ptr->UpdateChannel(this);}
