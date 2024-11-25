
#ifndef MESSGAGEBASE_HPP
#define MESSGAGEBASE_HPP
#include <iostream>
#include <string>
#include <chrono>
//���
class MessageBase
{
public:
    MessageBase(char* data):data(data)
    {
        //��ԭʼ���ݴӽ�������������

    }
    ~MessageBase()
    {

    }

    virtual void getFinishTime()
    {
        //�ȸ���ǰ�ڽ�����id��ʱ��
        this->m_ns = std::chrono::nanoseconds(1);
        this->m_finish = std::chrono::high_resolution_clock::now() + m_ns;
    }
    //����
    std::string data;
    //��Ϣ����
    std::string type;
    //ִ��ʱ��
    std::chrono::nanoseconds m_ns;
    //�������ʱ���
    std::chrono::time_point<std::chrono::high_resolution_clock> m_finish;
    //�����������ʱ����Ⱥ���ȷ�����ȼ�
    bool operator<(const MessageBase& other) const
    {
        return m_finish > other.m_finish;
    }
};
#endif