//
//  实现了http请求的解析
// ?:表示的是前面这个字符出现次数为0或1，也就是可有可无
// *:匹配0个或者多个
// +:匹配出现1次以上的字符合
// {}:限定某字符出现的具体次数，{2，6}:出现2~6次
// ():可以把多个字符打包起来做上面这些限定符的处理，C++
// regex库中给予了一个捕获得含义
// ?::可以屏蔽掉()的捕获含义
// |,^,&:表示他们原本的含义
// []:含义是匹配字符集，针对的是单个字符[a-z]:对于某字符限定规则是取得a-z字符
// 元字符:
// \d:代表的数字字符
// \w:代表英文数字下划线
// \s:代表空白符包括tab和\n
// \D,\W,\S:表示与上面小写相反的意思 .:代表任意字符，但是不包含换行符 ^:匹配行首
// $:匹配行尾,b$,匹配行尾有b的
// 贪婪匹配和懒惰匹配:<.*>这会尽量去匹配多的，贪婪会先去读取所有满足.*的，然后在从末尾吐出去直到找到>
// <.*?>加一个？表示懒惰匹配，一碰到>便会返回
#include <iostream>
#include <regex>
#include <string>

int main() 
{
    std::string src ="Get /baidu/login?user=HeJieY&pass=123 HTTP/1.1\r\n";
    std::smatch maches; // maches,这个用来表示提取出的每一个所要求的字符串,用法与数组类似
    std::regex e(R"((Get|Post|Put|Delete|Head) /([^?]+)\?(.*) (HTTP/(?:1\.0|1\.1|2\.0))(?:\r\n|\n)?)"); // 这个用来表示要提取字符串的正则表达式规则
    bool ret = std::regex_match(src, maches,e); // 从src字符串中按照e规则提取字符串至maches

    if (ret == false) 
    { 
        return -1;
    } 
    for(auto& s : maches)
    {
        std::cout << s << std::endl;
    }
    return 0;
}
