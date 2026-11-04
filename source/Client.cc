/*
 * @Author: HejieY 2145381080@qq.com
 * @Date: 2026-11-02 16:29:38
 * @FilePath: /muduo-http-server/source/Client.cc
 * @Description: 
 * Copyright (c) 2026 by HeJieY, All Rights Reserved. 
 */
#include"Server.hpp"
int main()
{
    Socket client;
    client.CreateSocket();
    client.CreateClientConnect("127.0.0.1", 8081);
    std::string buffer;
    
    buffer= "你好";
    while(true)
    {
        ssize_t ret = client.Send(&buffer[0], BUFFER_SIZE);
        if(ret < 0)
        {
            ERR_LOG("Send error!");
            return -1;
        }
        printf("%ld",ret);
        INF_LOG("Send a message!");
        sleep(2);
    }
    return  0;
}