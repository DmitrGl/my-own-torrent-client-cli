#include "byte_tools.h"
#include <openssl/sha.h>

int BytesToInt(std::string_view bytes) {
    return (static_cast<unsigned char>(bytes[0]) << 24) |
        (static_cast<unsigned char>(bytes[1]) << 16) |
        (static_cast<unsigned char>(bytes[2]) << 8) |
        static_cast<unsigned char>(bytes[3]);
}
uint32_t IpToInt(std::string_view ip){
    std::string wrd = "";
    uint32_t ans = 0;
    int btleft = 3;
    for(auto i : ip){
        if(i == '.'){
            ans += std::stoul(wrd)<<(8*btleft);
            btleft--;
            wrd = "";
            continue;
        }
        wrd += i;
    }
    ans += std::stoul(wrd) << (8 * btleft);
    btleft--;
    wrd = "";
    return ans;
}

std::string CalculateSHA1(const std::string& msg) {
    std::string c(20,0);
    SHA1((unsigned char*)msg.c_str(), msg.size(), (unsigned char*)c.data());
    return c;   
}

std::string toBE(int a) {
    std::string r;
    r.push_back(static_cast<char>((a >> 24) & 0xFF));
    r.push_back(static_cast<char>((a >> 16) & 0xFF));
    r.push_back(static_cast<char>((a >> 8) & 0xFF));
    r.push_back(static_cast<char>(a & 0xFF));
    return r;
}

std::string HexEncode(const std::string& input) {
    std::ostringstream oss;
    oss << std::hex << std::setfill('0');
    bool first = true;
    for (unsigned char byte : input) {
        if (!first) {
            oss << " ";
        }
        oss << std::setw(2) << static_cast<int>(byte);
        first = false;
    }
    
    return oss.str();
}