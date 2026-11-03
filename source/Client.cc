#include"Server.hpp"
int main()
{
    Socket client;
    client.CreateSocket();
    client.CreateClientConnect("118.89.172.104", 8081);
    std::string buffer;
    buffer= "你好";
    while(true)
    {
        client.Send(&buffer[0], BUFFER_SIZE);
        sleep(2);
    }
    return  0;
}