/*
 * @Author: HejieY 2145381080@qq.com
 * @Date: 2026-10-12 22:19:00
 * @FilePath: /muduo-http-server/http-v1/TimeWheel.cpp
 * @Description: 这是一个时间轮算法主要实现了任务的自动删除和延时
 * @Questions: 
 * 1. 关于类的初始化还不是很熟悉,声明和初始化的关系
 * 2. bind函数使用还存在疑问,为什么RemoveTask接受了参数绑定到ReleaseInfo上便是空参调用了
 * 3. 关于模板编程:函数模板,类模板还有问题
 * 4. 关于栈对象,堆对象,智能指针对象的内存管理得注意:什么时候消除,会不会自动析构,会不会自动构造
 * 5. 时间轮这里有一个重要的问题是:关于map中为什么要存weak_ptr这个很重要
 * 6. 类的函数是怎么存在的?类的虚函数表是什么?继承如何实现?虚继承是什么?这些问题存疑
 * 7. 装饰器底层是什么?bind底层是什么?这些是怎么运行起来的,待办
 * Copyright (c) 2026 by HeJieY, All Rights Reserved. 
 */
//  实现了时间论的算法
//  考虑到任务是自动删除的这说明：删除任务应该是代码内部自己的调用，所以我们把它
//封装，做到外部接口不调用他们，从而解耦内聚。

#include<functional>
#include <sys/types.h>
#include<unordered_map>
#include<cstdint>
#include <memory>
#include <iostream>
#include<unistd.h>
#include<thread>
#include<vector>

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
    To_Death_Cb _death_task;//用于表示时间到后自动执行的任务 
    ReleaseInfo _release;   //这个是回调用于删除map中的信息避免内存泄漏
public:
    TimeTask(const uint64_t id,const uint32_t timeout,uint32_t delay,const To_Death_Cb& death_task)
        :_id(id)
        ,_timeout(timeout)
        ,_death_task(death_task)
        ,_delay(delay)

    {

    }

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
    uint32_t GetTimeOut()
    {
        return _timeout;
    }

    uint32_t GetStdDelay()
    {
        return _delay;
    }
};


class TimeWheel
{
    using STaskPtr = std::shared_ptr<TimeTask>;
    using WTaskPtr = std::weak_ptr<TimeTask>;
private:
    std::vector<std::vector<STaskPtr>> _tasks;  //表示所有待办任务
    std::unordered_map<uint64_t,WTaskPtr> _map; //表示id与任务之间的联系
    uint64_t _capacity;                         //表示时间轮的最大容量
    uint32_t _ticks;                            //表示秒针 
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
public:
    TimeWheel(uint64_t  capacity)
        :_capacity(capacity)
        ,_ticks(0)
        ,_tasks(capacity)
    {
        
    }
    ~TimeWheel()
    {
        
    }

    int Reflash(uint64_t id)
    {
        auto it = _map.find(id);  
        if(it != _map.end())
        {
            //计算位置,并添加进入相应位置
            //这是存在问题的，原因在于从weak指针中拿取得原对象，新创建的
            //share_ptr不会共享引用计数
            //_tasks[pos].push_back(STaskPtr(it->second.lock()));            
            STaskPtr ptr = it->second.lock();
            int pos = (_ticks + ptr->GetStdDelay()) % _capacity;
            _tasks[pos].push_back(ptr); 
        }
        else
        {
            return -1;
        }
        return 0;
    }
    bool AddTask(const uint64_t id,const uint32_t timeout,int delay,const To_Death_Cb& cb)
    {
        //找到对应位置,添加要执行的任务去tasks中
        int pos = (_ticks + timeout) % _capacity;
        STaskPtr ptr = std::make_shared<TimeTask>(id,timeout,delay,cb);
        ptr->SetRelease(std::bind(&TimeWheel::RemoveTask,this,id));
        //这么写是不对的c++标准规定:非静态成员函数(不含this的)函数名不会自动强转成函数指针
        // ptr->SetRelease(std::bind(RemoveTask,this,id));
        _tasks[pos].push_back(ptr);
        // 添加到_map中加以管理
        _map[id] = WTaskPtr(ptr);
        
        return true;
    }
    bool TickMove()
    {
        _tasks[_ticks].clear();
        _ticks = (_ticks+1) % _capacity;
        return true;
    }
};


//Test部分
class Test
{
public:
    int _id;
    Test(int  id)
    {
        _id = id;
    }
    
    void DeathTask()
    {
        std::cout << "task over: " << _id << std::endl;
    }

    ~Test()
    {
        std::cout << "已经析构了" <<  std::endl;
    }

};


int main()
{
    TimeWheel tw(30);
    //子线程:
    std::thread t([]()->void{
        for(int i = 1;i<10;i++)
        {
            sleep(1);
            std::cout  << "已经经过" << i << "s" << std::endl; 
        }
    });
    //主线程:
    for(int i=1;i<30;i++)
    {
        Test* test = new Test(i);
        tw.AddTask(i,5,10,[test]()->void{ test->DeathTask(); delete test;});
        sleep(1);
        tw.TickMove();
    }


    t.join();
    return 0;
}




















