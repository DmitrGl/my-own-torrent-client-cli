#pragma once

#include <string>
#include <vector>

struct TorrentFile {
    std::vector<std::string> announce_list;
    std::string announce;
    std::string comment;
    std::vector<std::string> pieceHashes;
    size_t pieceLength;
    size_t length;
    std::string name;
    std::string infoHash;
};

TorrentFile LoadTorrentFile(const std::string& filename);
