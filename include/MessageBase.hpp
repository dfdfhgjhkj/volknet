
#ifndef MESSGAGEBASE_HPP
#define MESSGAGEBASE_HPP
#include <iostream>
#include <string>
#include <chrono>
//获得
class MessageBase
{
public:
    MessageBase(char* data):data(data)
    {
        //将原始数据从接收区复制下来

    }
    ~MessageBase()
    {

    }

    virtual void getFinishTime()
    {
        //先根据前期解析出id和时间
        this->m_ns = std::chrono::nanoseconds(1);
        this->m_finish = std::chrono::high_resolution_clock::now() + m_ns;
    }
    //数据
    std::string data;
    //消息类型
    std::string type;
    //执行时间
    std::chrono::nanoseconds m_ns;
    //期望完成时间点
    std::chrono::time_point<std::chrono::high_resolution_clock> m_finish;
    //根据期望完成时间点先后来确定优先级
    bool operator<(const MessageBase& other) const
    {
        return m_finish > other.m_finish;
    }
};
#endif