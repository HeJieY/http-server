/*
 * @Author: HejieY 2145381080@qq.com
 * @Date: 2026-10-13 18:18:45
 * @FilePath: /muduo-http-server/http-v1/Any.cpp
 * @Description: 这是一个any类可以接收任何类.利用了继承多态模板的思想
 * @Questions: 
 * 1.仔细体会这种类设计思想,感觉之前很少见
 * 2.关于父类指针对子类的引用,子类指针对父类的引用,情况都大不一样,注意区分 
 * Copyright (c) 2026 by HeJieY, All Rights Reserved. 
 */
#include<typeinfo>
#include<cstdio>
#include<cassert>
// 这种Any类的设计思想也可以多看看
class Any
{
private:
    class Holder
    {
        public:
            virtual ~Holder(){}
            virtual const std::type_info& GetType() = 0;
            virtual Holder* Clone() = 0;
    };
    template<typename T>
    class PlaceHolder : public Holder
    {
        public:
            T _data;
        public: 
            PlaceHolder(T data):_data(data){}
            ~PlaceHolder(){}
            const std::type_info& GetType() override
            { return typeid(_data); }
            Holder* Clone() override
            { return new PlaceHolder<T>(_data); } 
    };
    Holder* _content;
public:
    // 这个写法比较新，可以多看看
    void Swap(Any& other)
    { 
        Holder* tmp = this->_content;
        _content = other._content;
        other._content = tmp; 
    }
    Any():_content(nullptr) {}

    template<typename T>
    Any(const T& val)
        :_content(new PlaceHolder<T>(val)) {}
    Any(const Any& other)
        :_content(other._content ? other._content->Clone() : nullptr) {}
    Any& operator=(const Any& other)
    {
        if(this!=&other)
        {
            Any tmp(other);
            Swap(tmp);
        }
       return *this;
    }
    // 为什么当赋值重载碰上模板函数不用<>了？
    // 函数模板当给予实参时，会自动退到类型，不用显示<>
    template<typename T>
    Any& operator=(const T& val)
    {
       Any(val).Swap(*this);
       return *this;
    }

    ~Any() { delete _content; }
    // 关于多态：
    // 子类指针访问父类时进行的是对象切片，不需要进行强转，这是安全的
    // 父类指针访问子类时:对于发生多态的函数不用强转，对于子类自己的函数则必须进行强转 
    template<typename T> 
    T GetData()
    {
        assert(typeid(T) == _content->GetType());
        return ((PlaceHolder<T>*)_content)->_data;     
    }
};

int main()
{
    Any a = 10;
    printf("%d\n",(a.GetData<int>()));
    return 0;
}

