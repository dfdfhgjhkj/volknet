#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <map>
#include <functional>
struct Signal 
{
    std::string name;
    int startBit;
    int bitLength;
    std::string unit;
    double minValue;
    double maxValue;
    int offset;
    int factor;
    //0:litter 1:big
    int order;
    //0:有符号 1:无符号
    int signal;
    //1:float 2:double
    int floatNum=0;
};

struct Message 
{
    int id;
    std::string name;
    int dlc;  // Data Length Code
    std::vector<Signal> signals;
};

class DbcParser 
{
public:
    bool parse(const std::string& filename);
    void printMessages();

private:
    std::map<int,Message> messages;
    int nowMessageID;
    void parseMessage(std::istringstream& lineStream, Message& message);
    void parseSignal(std::istringstream& lineStream);
    void parseFloatOrDouble(std::istringstream& lineStream);
    void parseCANMessage(int messageId, const std::vector<uint8_t>& data, const std::vector<Message>& messages);
    uint64_t extractSignalValue(const std::vector<uint8_t>& data, const Signal& signal);

};

bool DbcParser::parse(const std::string& filename) {
    std::ifstream file(filename);
    std::string line;

    if (!file.is_open()) 
    {
        std::cerr << "Failed to open file: " << filename << std::endl;
        return false;
    }
    int flag = 0;
    while (std::getline(file, line)) 
    {
        std::istringstream lineStream(line);
        std::string token;

        lineStream >> token;

        if (token == "BO_") 
        {            
            Message newMessage;            
            parseMessage(lineStream, newMessage);
            this->messages[newMessage.id]=newMessage;
            nowMessageID = newMessage.id;
        }
        if (token == "SG_")
        {
            parseSignal(lineStream);
        }
        if (token == "SIG_VALTYPE_")
        {
            parseFloatOrDouble(lineStream);
        }

    }

    return true;
}
void DbcParser::parseFloatOrDouble(std::istringstream& lineStream)
{
    std::string token;
    int messageId;
    std::string signalName;
    int floatOrDouble;
    lineStream >> token;
    messageId = std::stoi(token);
    lineStream >> token;
    signalName = token;
    lineStream >> token;
    lineStream >> token;
    floatOrDouble = std::stoi(token);
    for  (auto i = this->messages[messageId].signals.begin(); i!= this->messages[messageId].signals.end();i++)
    {
        if (i->name== signalName)
        {
            i->floatNum = floatOrDouble;
        }

    }
}
void DbcParser::parseMessage(std::istringstream& lineStream, Message& message) 
{
    //Message message;
    std::string token;
    lineStream >> message.id >> message.name >> token;  // BO_ 1234 MessageName: 8 Vector__XXX
    message.dlc = std::stoi(token);
}

void DbcParser::parseSignal(std::istringstream& lineStream) 
{
    Signal signal;
    std::string token;
    lineStream >> signal.name;
    lineStream >> token;  
    lineStream >> token;// 0|8@1+ (1,0) [0|255] "units"
    signal.startBit = std::stoi(token.substr(0, token.find('|')));
    signal.bitLength = std::stoi(token.substr(token.find('|') + 1, token.find('@') - token.find('|') - 1));
    signal.order =std::stoi(token.substr(token.find('@') + 1));
    signal.signal= token.substr(token.find('@') + 2)=="+"?1:0;
    lineStream >> token;  
    signal.factor = std::stod(token.substr(token.find('(') + 1, token.find(')') - token.find('(') - 1));
    signal.offset = std::stod(token.substr(token.find(',') + 1));
    lineStream >> token;// Optional: [0|255]
    signal.minValue = std::stoi(token.substr(token.find('[') + 1, token.find(']') - token.find('[') - 1));
    signal.maxValue = std::stoi(token.substr(token.find('|') + 1));
    lineStream >> token;
    signal.unit = token.substr(token.find('[') + 1, token.find(']') - token.find('[') - 1);
    lineStream >> token;
    this->messages[nowMessageID].signals.push_back(signal);
}

void DbcParser::printMessages() 
{
    for (const auto& message : messages) 
    {
        std::cout << "Message ID: " << message.second.id << ", Name: " << message.second.name << ", DLC: " << message.second.dlc << std::endl;
        for (const auto& signal : message.second.signals)
        {
            std::cout << "    Signal: " << signal.name << ", Start Bit: " << signal.startBit << ", Bit Length: " << signal.bitLength
                << ", Unit: " << signal.unit << ", Range: [" << signal.minValue << ", " << signal.maxValue << "]" << "offset: " <<signal.offset<<"factor: "<<signal.factor << std::endl;
        }
    }
}

// 解析一个字节数组中的信号
uint64_t DbcParser::extractSignalValue(const std::vector<uint8_t>& data, const Signal& signal)
{
    uint64_t value = 0;

    // 计算信号的起始字节索引和位偏移量
    int byteIndex = signal.startBit / 8;
    int bitOffset = signal.startBit % 8;

    // 从数据中提取信号的值
    for (int i = 0; i < signal.bitLength; ++i) 
    {
        // 获取当前字节
        uint8_t currentByte = data[byteIndex];
        // 提取当前位
        uint8_t bitValue = (currentByte >> (bitOffset + i)) & 1;
        // 将提取的位值加入到最终的value中
        value |= (bitValue << i);

        // 如果该信号跨越了多个字节，则移动到下一个字节
        if ((bitOffset + i) / 8 != byteIndex) {
            byteIndex++;
        }
    }

    // 如果信号是有符号的，则进行符号扩展
    if (signal.signal && value & (1 << (signal.bitLength - 1))) 
    {
        value |= (~0ULL << signal.bitLength); // 符号扩展
    }

    return value;
}

void DbcParser::parseCANMessage(int messageId, const std::vector<uint8_t>& data, const std::vector<Message>& messages)
{
    for (const auto& message : messages) 
    {
        if (message.id == messageId) 
        {
            std::cout << "Message: " << message.name << std::endl;
            for (const auto& signal : message.signals) {
                uint64_t value = extractSignalValue(data, signal);
                std::cout << "    Signal: " << signal.name << ", Value: " << value << std::endl;
            }
        }
    }
}
