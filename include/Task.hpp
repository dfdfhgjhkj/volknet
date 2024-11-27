#include <string>
#include "MessageBase.hpp"
struct MessageTask
{
    MessageTask()
    {

    }
    //改成移动右值
    MessageTask(const std::string&& MessageTaskName, std::function<void(std::shared_ptr<MessageBase>)>& Function)
    {
        this->MessageTaskName = MessageTaskName;

        this->Function = std::make_shared<std::function<void(std::shared_ptr<MessageBase>)>>(Function);
    }
    std::string MessageTaskName;
    std::shared_ptr<std::function<void(std::shared_ptr<MessageBase>)>> Function;
};

struct Task
{
    Task()
    {

    }
    //改成移动右值
    Task(const std::string&& TaskName, std::function<void()>& Function)
    {
        this->TaskName = TaskName;

        this->Function = std::make_shared<std::function<void()>>(Function);
    }
    std::string TaskName;
    std::shared_ptr<std::function<void()>> Function;
};
