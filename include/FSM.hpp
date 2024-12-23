// FSM.cpp : 此文件包含 "main" 函数。程序执行将在此处开始并结束。
//
#include <iostream>
#include <string>
#include <vector>
#include <unordered_set>
#include <functional>
#include <map>

//State转换Event
class BaseEvent
{
public:
    BaseEvent();
    BaseEvent(const std::string&& m_eventName, int m_eventComparison, double m_value);
    virtual ~BaseEvent();

    const std::string&& getName() const;
    BaseEvent& operator=(const BaseEvent& baseEvent);
    const bool operator==(const BaseEvent& baseEvent)const;
    const bool operator<(const BaseEvent& other) const;
    // 重载 hash 函数
    struct Hash
    {
        std::size_t operator()(BaseEvent const& s) const noexcept
        {
            std::size_t h1 = std::hash<std::string>{}(s.m_eventName);
            std::size_t h2 = std::hash<int>{}(s.m_eventComparison);
            std::size_t h3 = std::hash<double>{}(s.m_value);
            return h1 ^ (h2 << 1) ^ (h3 << 2); //
        }
    };


private:

    //Event名称
    std::string m_eventName;
    //Event ==1 !=2 >3 <4 >=5 <=6
    int m_eventComparison;
    //Event  value
    double m_value;
};

BaseEvent::BaseEvent()
{
    this->m_eventName = "";
    this->m_eventComparison = 0;
    this->m_value = 0.0;

}
BaseEvent::BaseEvent(const std::string&& eventName, int eventComparison, double value)
{
    this->m_eventName = eventName;
    this->m_eventComparison = eventComparison;
    this->m_value = value;
}
BaseEvent::~BaseEvent()
{

}
const std::string&& BaseEvent::getName() const
{
    return (m_eventName + "_" + std::to_string(m_eventComparison) + "_" + std::to_string(m_eventComparison));
}
BaseEvent& BaseEvent::operator=(const BaseEvent& baseEvent)
{
    if (this != &baseEvent)
    {
        this->m_eventName = baseEvent.m_eventName;
        this->m_eventComparison = baseEvent.m_eventComparison;
        this->m_value = baseEvent.m_value;
    }
    return *this;

}
const bool BaseEvent::operator==(const BaseEvent& baseEvent)const
{
    if (
        this->m_eventName == baseEvent.m_eventName &&
        this->m_eventComparison == baseEvent.m_eventComparison &&
        this->m_value == baseEvent.m_value
        )
    {
        return true;
    }
    else
    {
        return false;
    }

}

// 比较函数
const bool BaseEvent::operator<(const BaseEvent& baseEvent) const
{
    if (baseEvent.m_eventName != this->m_eventName)
    {
        return (baseEvent.m_eventName < this->m_eventName);
    }
    else
    {
        if (baseEvent.m_eventComparison != this->m_eventComparison)
        {
            return (baseEvent.m_eventComparison < this->m_eventComparison);
        }
        else
        {
            if (baseEvent.m_value != this->m_value)
            {
                return (baseEvent.m_eventName < this->m_eventName);
            }
            else
            {
                return false;
            }
        }

    }
}

//State
class BaseState
{
public:
    BaseState();
    BaseState(const std::string&& stateName, const std::vector<std::string>&& actionNameVector);
    virtual ~BaseState();
    BaseState& operator=(const BaseState& baseState);
    const bool operator==(const BaseState& baseState)const;
    virtual void entry(const std::string&& stateMachineName, const std::string&& holderName) const;

    virtual void exit(const std::string&& stateMachineName, const std::string&& holderName) const;    
    const std::string&& getName() const;
    // 重载 hash 函数
    struct Hash
    {
        std::size_t operator()(BaseState const& s) const noexcept
        {
            std::size_t h1 = std::hash<std::string>{}(s.m_stateName);
            return h1; // or use boost::
        }
    };


private:
    //状态名称
    std::string m_stateName;
    //action名称Vector
    std::vector<std::string> m_actionNameVector;
};
BaseState::BaseState()
{
    this->m_stateName = "";
}
BaseState::BaseState(const std::string&& stateName, const std::vector<std::string>&& actionNameVector)
{
    this->m_stateName = stateName;
    this->m_actionNameVector = actionNameVector;
}
BaseState::~BaseState()
{

}
BaseState& BaseState::operator=(const BaseState& baseState)
{
    if (this != &baseState)
    {
        this->m_stateName = baseState.m_stateName;
        this->m_actionNameVector = baseState.m_actionNameVector;


    }
    return *this;

}
const bool BaseState::operator==(const BaseState& baseState)const
{
    if (
        this->m_stateName == baseState.m_stateName &&
        this->m_actionNameVector == baseState.m_actionNameVector
        )
    {
        return true;
    }
    else
    {
        return false;
    }

}
const std::string&& BaseState::getName() const
{
    return std::move(m_stateName);
}
void BaseState::entry(const std::string&& stateMachineName, const std::string&& holderName) const
{

    //进入新状态,通知单位设置新action
    std::cout << "State machine: " << stateMachineName << " holder: " << holderName << " state " << m_stateName << " entry." << std::endl;
    std::cout << "Actions:" << std::endl;
    for (std::vector<std::string>::const_iterator it = m_actionNameVector.begin(); it != m_actionNameVector.end(); it++)
    {
        std::cout << *it << " ";


    }

}
void BaseState::exit(const std::string&& stateMachineName, const std::string&& holderName)  const
{
    //进入新状态,通知单位设置新action
    std::cout << "State machine: " << stateMachineName << " holder: " << holderName << " state " << m_stateName << " entry." << std::endl;
    std::cout << "Actions:" << std::endl;
    for (std::vector<std::string>::const_iterator it = m_actionNameVector.begin(); it != m_actionNameVector.end(); it++)
    {
        std::cout << *it << " ";

    }
    std::cout << "exit" << std::endl;

}





class BaseFSM
{
public:
    BaseFSM();
    BaseFSM(const std::string&& FSMName);
    const std::string&& getName() const;
    virtual ~BaseFSM();
    //设置持有者的状态接口
    int setHolderState(const std::string&& holderName, const BaseState&& state);
    //添加Event接口
    int addEvent(const BaseEvent&& event);
    //添加States接口
    int addState(const BaseState&& state);
    //设置事件状态转移函数接口
    int setEventGuardFunction(const BaseState&& p_state, const BaseEvent&& event, const BaseState&& n_state);
    //传递Event接口
    int addHolderEvent(const std::string&& holderName, const BaseEvent&& event);

private:
    //持有者名称State列表
    std::map<std::string, const BaseState*> m_holderStateMap;
    //Event状态转移函数指针Map
    std::map<BaseEvent, std::function<int(std::string)>> m_stateNameGuardPtrVectorMap;
    //Event列表
    std::unordered_set<BaseEvent, BaseEvent::Hash>  m_eventSet;
    //State列表
    std::unordered_set<BaseState, BaseState::Hash>  m_stateSet;
    //状态机名称
    std::string m_FSMName;
};

const std::string&& BaseFSM::getName() const
{
    return std::move(m_FSMName);
}
//添加Event接口
int BaseFSM::addEvent(const BaseEvent&& event)
{
    m_eventSet.insert(event);
    return 0;
}
//添加States接口
int BaseFSM::addState(const BaseState&& state)
{
    m_stateSet.insert(state);
    return 0;
}
//设置持有者状态
int BaseFSM::setHolderState(const std::string&& holderName, const BaseState&& state)
{
    std::unordered_set<BaseState, BaseState::Hash>::iterator it = m_stateSet.find(state);
    if (it != m_stateSet.end())
    {

    }
    else
    {
        std::cout << "State machine: " << m_FSMName << " no state " << state.getName() << "." << std::endl;
        return 2;
    }
    if (m_holderStateMap.count(holderName))
    {

        m_holderStateMap[holderName] = &(*it);
    }
    else
    {
        m_holderStateMap.insert(std::make_pair(holderName, &(*it)));
    }
    return 0;

}
int BaseFSM::setEventGuardFunction(const BaseState&& pBaseState, const BaseEvent&& baseEvent, const BaseState&& nBaseState)
{
    std::unordered_set<BaseState, BaseState::Hash>::iterator pStateIt;
    std::unordered_set<BaseState, BaseState::Hash>::iterator nStateIt;
    std::function<bool(std::unordered_set<BaseState, BaseState::Hash>::iterator&, const BaseState&)> findState
        = [&](std::unordered_set<BaseState, BaseState::Hash>::iterator& itState, const BaseState& state)
        {
            itState = m_stateSet.find(state);
            if (itState != m_stateSet.end())
            {
                return true;
            }
            else
            {
                std::cout << "State machine: " << m_FSMName << " no state " << state.getName() << "." << std::endl;
                return false;
            }

        };
    if (!findState(pStateIt, pBaseState) || !findState(nStateIt, nBaseState))
    {
        return 2;
    }

    std::unordered_set<BaseEvent, BaseEvent::Hash>::iterator EventIt;
    std::function<bool(std::unordered_set<BaseEvent, BaseEvent::Hash>::iterator&, const BaseEvent&)> findEvent
        = [&](std::unordered_set<BaseEvent, BaseEvent::Hash>::iterator& itEvent, const BaseEvent& event)
        {
            itEvent = m_eventSet.find(event);
            if (itEvent != m_eventSet.end())
            {
                return true;
            }
            else
            {
                std::cout << "State machine: " << m_FSMName << " no event " << event.getName() << "." << std::endl;
                return false;
            }

        };
    if (!findEvent(EventIt, baseEvent))
    {
        return 3;
    }


    //设置lambda表达式
    std::function<int( BaseFSM*, std::string, std::unordered_set<BaseState, BaseState::Hash>::iterator, std::unordered_set<BaseState, BaseState::Hash>::iterator)>
        guard_lambda =
        [&]( BaseFSM* baseFSM, std::string  holderName, std::unordered_set<BaseState, BaseState::Hash>::iterator pStateIt, std::unordered_set<BaseState, BaseState::Hash>::iterator nStateIt)
        {
            if ((*pStateIt) == *(baseFSM->m_holderStateMap[holderName]))
            {
                pStateIt->entry(baseFSM->getName(), std::move(holderName));
                baseFSM->m_holderStateMap[holderName] = &(*nStateIt);
                nStateIt->exit(baseFSM->getName(), std::move(holderName));
                return 0;
            }

        };
    std::function<int(std::string)> guardFunction = std::bind(guard_lambda, this,std::placeholders::_1, pStateIt, nStateIt);
    m_stateNameGuardPtrVectorMap[baseEvent] = guardFunction;
    return 0;

}
//传递Event接口
int BaseFSM::addHolderEvent(const std::string&& holderName, const BaseEvent&& event)
{
    std::unordered_set<BaseEvent, BaseEvent::Hash>::iterator EventIt;
    std::function<bool(std::unordered_set<BaseEvent, BaseEvent::Hash>::iterator&, const BaseEvent&)> findEvent
        = [&](std::unordered_set<BaseEvent, BaseEvent::Hash>::iterator& itEvent, const BaseEvent& event)
        {
            itEvent = m_eventSet.find(event);
            if (itEvent != m_eventSet.end())
            {
                return true;
            }
            else
            {
                std::cout << "State machine: " << m_FSMName << " no event " << event.getName() << "." << std::endl;
                return false;
            }

        };
    if (!findEvent(EventIt, event))
    {
        return 3;
    }
    if (m_stateNameGuardPtrVectorMap.count(event))
    {
        m_stateNameGuardPtrVectorMap[event](holderName);
        return 0;
    }
    else
    {
        std::cout << "Event" << event.getName() << " no guardFunction." << std::endl;
        return 4;
    }

}
BaseFSM::BaseFSM()
{
    m_FSMName = "";
}
BaseFSM::BaseFSM(const std::string&& FSMName)
{
    m_FSMName = FSMName;
}
BaseFSM::~BaseFSM()
{
}



/*
int main()
{
    std::shared_ptr <BaseFSM> SPBaseFSM = std::make_shared<BaseFSM>("newFSM");
    std::map<std::string, std::shared_ptr <BaseFSM>> fsmMap;
    fsmMap.insert(std::make_pair("newFSM", SPBaseFSM));

    BaseEvent newEvent("1", 1, 0.0);

    std::vector<std::string> newState1Actions;
    newState1Actions.push_back("1");
    newState1Actions.push_back("2");
    BaseState newState1("newState1", std::move(newState1Actions));

    std::vector<std::string> newState2Actions;
    newState2Actions.push_back("3");
    newState2Actions.push_back("4");
    BaseState newState2("newState2", std::move(newState2Actions));

    fsmMap["newFSM"]->addState(std::move(newState1));
    fsmMap["newFSM"]->addState(std::move(newState2));
    fsmMap["newFSM"]->addEvent(std::move(newEvent));

    fsmMap["newFSM"]->setHolderState("one", std::move(newState1));
    fsmMap["newFSM"]->setEventGuardFunction(std::move(newState1), std::move(newEvent), std::move(newState2));
    fsmMap["newFSM"]->addHolderEvent("one", std::move(newEvent));

}
*/
