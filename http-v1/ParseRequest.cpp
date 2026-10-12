//
//  实现了http请求的解析
#include<regex>
#include<iostream>
#include<string>
int main()
{
    std::string src = "Get http://www.baidu.com/login?username=admin&password=123456\r\n";
    std::smatch maches; //maches,这个用来表示提取出的每一个所要求的字符串,用法与数组类似
    std::regex e("");//这个用来表示要提取字符串的正则表达式规则
    bool ret = std::regex_match(src,maches,e);//从src字符串中按照e规则提取字符串至maches
    
    if(ret == false)
    {
        return -1;
    }
    else 
    {
        return 0;
    }
    return 0;
}