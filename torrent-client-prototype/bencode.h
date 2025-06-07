#pragma once

#include <string>
#include <vector>
#include <fstream>
#include <variant>
#include <list>
#include <map>
#include <sstream>
#include <algorithm>

namespace Bencode {
    /*
     * Рекурсивно выделить содержимое после указанного индекса 
     */
    std::string ParseInfo(const std::string& file, size_t& ind);
    
    /*
     * Функция ищет возвращает в строке то, что лежит по ключу
     */
    std::string FindInTorrent(const std::string &file, std::string what);

    /*
     * Возвращает обработанный announce-list
     */
    std::vector<std::string> ParseAnnounceList(const std::string& announce_list_str);
}
