#include "torrent_file.h"
#include "bencode.h"
#include <vector>
#include <openssl/sha.h>
#include <fstream>
#include <variant>
#include <sstream>

TorrentFile LoadTorrentFile(const std::string& filename) {
    TorrentFile torfile;
    std::ifstream file(filename);
    std::ostringstream ss;
    ss << file.rdbuf();
    std::string filestr = ss.str();
    std::string announce = Bencode::FindInTorrent(filestr, "announce");
    torfile.announce = announce;
    std::string comment = Bencode::FindInTorrent(filestr, "comment");
    torfile.comment = comment;
    std::string piece_len = Bencode::FindInTorrent(filestr, "piece length");
    torfile.pieceLength = std::stoull(piece_len);
    std::string len = Bencode::FindInTorrent(filestr, "length");
    torfile.length = std::stoull(len);
    std::string name = Bencode::FindInTorrent(filestr, "name");
    torfile.name = name;
    std::string hashes = Bencode::FindInTorrent(filestr, "pieces");
    std::string wrd = "";
    for(size_t i = 0; i<hashes.size(); i++){
        if(i%20 == 0 && i!= 0){
            torfile.pieceHashes.push_back(wrd);
            wrd.clear();
            wrd += hashes[i];
            continue;
        }
        wrd += hashes[i];
    }
    if(!wrd.empty())torfile.pieceHashes.push_back(wrd);
    std::string info = Bencode::FindInTorrent(filestr, "info");
    std::string c(20,0);
    SHA1((unsigned char*)info.c_str(), info.size(), (unsigned char*)c.data());
    torfile.infoHash = c;

    std::string announce_list = Bencode::FindInTorrent(filestr, "announce-list");
    if(!announce_list.empty()){
        torfile.announce_list = Bencode::ParseAnnounceList(announce_list);
    }

    file.close();

    return torfile;
}
