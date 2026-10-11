#include<functional>
#include<unordered_map>
#include<cstdint>
#include <memory>

using To_Death_Cb = std::function<void()>;
using ReleaseInfo = std::function<void()>;
class TimeTask
{
private:
    uint64_t _id;           //用于唯一标识任务
    uint32_t _timeout;      //用于标识任务的生命时间
    To_Death_Cb _death_task;//用于表示时间到后自动执行的任务 
    ReleaseInfo _release; 
    // task_cb _task;
public:
    TimeTask(const uint64_t id,const uint32_t timeout)
        :_id(id)
        ,_timeout(timeout)
        //,_task(task)
    {

    }

    ~TimeTask()
    {
        _death_task();
    }

    void SetDeathFunc(To_Death_Cb death_task)
    {
        _death_task = death_task;
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
public:
    TimeWheel(uint64_t  capacity)
        :_capacity(capacity)
        ,_ticks(0)
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
            int pos = (_ticks+_capacity) % _capacity;
            _tasks[pos].push_back(STaskPtr(it->second.lock()));
        }
        else
        {
            return -1;
        }
        return 0;
    }
    bool AddTask(const uint64_t id,const uint32_t timeout,const To_Death_Cb& cb)
    {
        //找到对应位置,添加要执行的任务去tasks中
        int pos = (_ticks+timeout) % _capacity;
        STaskPtr ptr = std::make_shared<TimeTask>(id,timeout);
        _tasks[pos].push_back(ptr);
        // 添加到_map中加以管理
        _map[id] = WTaskPtr(ptr);

        return true;
    }
    bool TickMove()
    {
        _ticks =  (_ticks = _ticks+1) % _capacity;

        _tasks[_ticks].clear();
        return true;
    }
};





















