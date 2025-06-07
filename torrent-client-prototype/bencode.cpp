#include "bencode.h"

namespace Bencode {
    std::string ParseInfo(const std::string& file, size_t& ind){
        int n = file.size();
        std::string retval = "";
        if(file[ind] == 'd'){
            retval += file[ind];
            ind++;
            while(file[ind] != 'e'){
                std::string szwrd = "";
                for(ind; ind<n; ind++){
                    if(file[ind] == ':')break;
                    szwrd+= file[ind];
                    retval += file[ind];
                }
                retval += file[ind];
                ind++;
                size_t sz = std::stoull(szwrd);
                for(size_t i = ind; i<ind+sz; i++){
                    retval += file[i];
                }
                ind += sz;
                retval += ParseInfo(file, ind);
            }
            retval += file[ind];
            ind++;
            return retval;
        }
        if(file[ind] == 'i'){
            while(file[ind]!= 'e'){
                retval += file[ind];
                ind++;
            }
            retval += file[ind];
            ind++;
            return retval;
        }
        if(file[ind] == 'l'){
            
            retval += file[ind];
            ind++;
            while(file[ind]!= 'e'){
                retval += ParseInfo(file, ind);
            }
            retval += file[ind];
            ind++;
            return retval;
        }
        std::string szwrd = "";
        for(ind; ind<n; ind++){
            if(file[ind] == ':')break;
            szwrd += file[ind];
            retval += file[ind];
        }
        retval += file[ind];
        ind++;
        size_t sz = std::stoull(szwrd);
        for(size_t i = ind; i<ind+sz; i++){
            retval += file[i];
        }
        ind += sz;
        return retval;
    }

    std::string FindInTorrent(const std::string &file, std::string what){
        size_t n = file.size();
        size_t m = what.size();
        std::string wrd = "";
        for(size_t i = 0; i<m;i++){
            wrd+= file[i];
        }
        bool fl = 1;
        size_t ans;
        for(size_t i = m; i<n;i++){
            if(wrd == what){
                if(file[i-m-1] != ':')continue;
                
                ans = i;
                fl = 0;
                break;
            }
            wrd.erase(wrd.begin());
            wrd+= file[i];
        }
        if(fl)return "";
        if(file[ans] == 'i'){
            std::string retval = "";
            for(size_t i = ans+1; i<n;i++){
                if(file[i] == 'e')break;
                retval += file[i];
            }
            return retval;
        }
        if(file[ans] == 'd' || file[ans] == 'l'){
            
            std::string retval = ParseInfo(file, ans);
            return retval;
        }
    
        std::string retszstr = "";
        for(size_t i = ans; i<n;i++){
            if(file[i] == ':'){
                ans = i+1;
                break;
            }
            retszstr+= file[i];
        }
        size_t retsz = std::stoull(retszstr);
        std::string retval = "";
        for(size_t i = ans; i < ans + retsz;i++){
            retval += file[i];
        }
        return retval;
    }

    std::vector<std::string> ParseAnnounceList(const std::string& announce_list_str) {
        std::vector<std::string> trackers;
        size_t pos = 0;
        pos++;

        while (pos < announce_list_str.size() && announce_list_str[pos] != 'e') {
            pos++;

            while (pos < announce_list_str.size() && announce_list_str[pos] != 'e') {
                size_t colon_pos = announce_list_str.find(':', pos);
                std::string length_str = announce_list_str.substr(pos, colon_pos - pos);
                int length = 0;
                
                length = std::stoi(length_str);
                

                pos = colon_pos + 1;
                std::string tracker = announce_list_str.substr(pos, length);
                trackers.push_back(tracker);

                pos += length;
            }
            if (pos < announce_list_str.size() && announce_list_str[pos] == 'e') {
                pos++;
            }
        }

        return trackers;
    }
}
