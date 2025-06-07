#include "torrent_tracker.h"
#include "bencode.h"
#include "byte_tools.h"
#include <cpr/cpr.h>
#include <iostream>

const std::vector<Peer> &TorrentTracker::GetPeers() const {
    return peers_;
}

TorrentTracker::TorrentTracker(const std::string& url):url_(url), peers_(0)
{
}

void TorrentTracker::UpdatePeers(const TorrentFile& tf, std::string peerId, int port){
    std::vector<std::string> all_trackers;
    if (!tf.announce.empty()) {
        all_trackers.push_back(tf.announce);
    }
    for (const auto& tracker : tf.announce_list) {
        all_trackers.push_back(tracker);
    }
    
    for (const auto& tracker_url : all_trackers) {
        try {
            std::cout << "Trying tracker: " << tracker_url << std::endl;
            url_ = tracker_url;
            cpr::Response res = cpr::Get(
                cpr::Url{url_},
                cpr::Parameters{
                    {"info_hash", tf.infoHash},
                    {"peer_id", peerId},
                    {"port", std::to_string(port)},
                    {"uploaded", "0"},
                    {"downloaded", "0"},
                    {"left", std::to_string(tf.length)},
                    {"compact", "1"}
                },
                cpr::Timeout{5000}
            );
            
            if (res.status_code == 0) {
                throw std::runtime_error("Connection failed");
            }
            
            if (res.status_code != 200) {
                throw std::runtime_error("HTTP status " + std::to_string(res.status_code));
            }
            
            if (res.text.empty()) {
                throw std::runtime_error("Empty response");
            }
            std::string peers_data = Bencode::FindInTorrent(res.text, "peers");
            if (peers_data.empty()) {
                throw std::runtime_error("No peers in response");
            }
            
            peers_.clear();
            
            for (size_t i = 0; i * 6 < peers_data.size(); i++) {
                const unsigned char* peer_bytes = reinterpret_cast<const unsigned char*>(peers_data.data() + i * 6);
                
                std::string ip = 
                    std::to_string(peer_bytes[0]) + "." +
                    std::to_string(peer_bytes[1]) + "." +
                    std::to_string(peer_bytes[2]) + "." +
                    std::to_string(peer_bytes[3]);
                
                int peer_port = (peer_bytes[4] << 8) | peer_bytes[5];
                
                peers_.emplace_back(Peer{ip, peer_port});
            }
            
            std::cout << "Successfully got " << peers_.size() << " peers from " << tracker_url << std::endl;
            return;
            
        } catch (const std::exception& e) {
            std::cerr << "Failed to connect to tracker " << tracker_url << ": " << e.what() << std::endl;
            continue;
        }
    }
    throw std::runtime_error("All trackers failed");
}