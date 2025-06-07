#include <sstream>
#include "message.h"
#include "byte_tools.h"
#include <arpa/inet.h>
#include <iostream>

Message Message::Parse(const std::string& messageString){
    Message res;
    if (messageString.empty()) {
        res.id = MessageId::KeepAlive;
        res.payload = "";
        res.messageLength = 0;
        return res;
    }
    res.messageLength = messageString.size();    
    res.id = static_cast<MessageId>(static_cast<uint8_t>(messageString[0]));
    if(messageString.size() < 2)return res;
    res.payload = messageString.substr(1);
    return res;
}

Message Message::Init(MessageId id, const std::string& payload){
    Message res;
    res.id = id;
    
    if (id == MessageId::KeepAlive) {
        res.messageLength = 0;
        res.payload = "";
    } else {
        res.messageLength = 1 + payload.size();
        res.payload = payload;
    }
    
    return res;
}

std::string Message::ToString() const {
        std::string result;
        uint32_t be_length = htonl(static_cast<uint32_t>(messageLength));
        result.append(reinterpret_cast<const char*>(&be_length), 4);
        if (id == MessageId::KeepAlive) {
            return result;
        }
        result.push_back(static_cast<char>(id));
        result += payload;
        return result;
    }
