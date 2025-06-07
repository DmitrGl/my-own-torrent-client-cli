#include "torrent_tracker.h"
#include "piece_storage.h"
#include "peer_connect.h"
#include "byte_tools.h"
#include <cassert>
#include <iostream>
#include <filesystem>
#include <random>
#include <thread>
#include <algorithm>

namespace fs = std::filesystem;

std::mutex cerrMutex, coutMutex;

std::string RandomString(size_t length) {
    std::random_device random;
    std::string result;
    result.reserve(length);
    for (size_t i = 0; i < length; ++i) {
        result.push_back(random() % ('Z' - 'A') + 'A');
    }
    return result;
}

const std::string PeerId = "TESTAPPDONTWORRY" + RandomString(4);
size_t PiecesToDownload;

void CheckDownloadedPiecesIntegrity(const std::filesystem::path& outputFilename, const TorrentFile& tf, PieceStorage& pieces) {
    pieces.CloseOutputFile();

    if (std::filesystem::file_size(outputFilename) != tf.length) {
        throw std::runtime_error("Output file has wrong size");
    }

    if (pieces.GetPiecesSavedToDiscIndices().size() != pieces.PiecesSavedToDiscCount()) {
        throw std::runtime_error("Cannot determine real amount of saved pieces");
    }

    if (pieces.PiecesSavedToDiscCount() < PiecesToDownload) {
        throw std::runtime_error("Downloaded pieces amount is not enough");
    }

    std::vector<size_t> pieceIndices = pieces.GetPiecesSavedToDiscIndices();
    std::sort(pieceIndices.begin(), pieceIndices.end());

    std::ifstream file(outputFilename, std::ios_base::binary);
    for (size_t pieceIndex : pieceIndices) {
        const std::streamoff positionInFile = pieceIndex * tf.pieceLength;
        file.seekg(positionInFile);
        if (!file.good()) {
            throw std::runtime_error("Cannot read from file");
        }
        std::string pieceDataFromFile(tf.pieceLength, '\0');
        file.read(pieceDataFromFile.data(), tf.pieceLength);
        const size_t readBytesCount = file.gcount();
        pieceDataFromFile.resize(readBytesCount);
        const std::string realHash = CalculateSHA1(pieceDataFromFile);

        if (realHash != tf.pieceHashes[pieceIndex]) {
            std::cerr << "File piece with index " << pieceIndex << " started at position " << positionInFile <<
                      " with length " << pieceDataFromFile.length() << " has wrong hash " << HexEncode(realHash) <<
                      ". Expected hash is " << HexEncode(tf.pieceHashes[pieceIndex]) << std::endl;
            throw std::runtime_error("Wrong piece hash");
        }
    }

    std::cout << "Pieces are correct" << std::endl;
}


bool RunDownloadMultithread(PieceStorage& pieces, const TorrentFile& torrentFile, const std::string& ourId, const TorrentTracker& tracker) {
    using namespace std::chrono_literals;

    std::vector<std::thread> peerThreads;
    std::vector<std::shared_ptr<PeerConnect>> peerConnections;

    for (const Peer& peer : tracker.GetPeers()) {
        peerConnections.emplace_back(std::make_shared<PeerConnect>(peer, torrentFile, ourId, pieces));
    }

    peerThreads.reserve(peerConnections.size());

    for (auto& peerConnectPtr : peerConnections) {
        peerThreads.emplace_back(
                [peerConnectPtr] () {
                    bool tryAgain = true;
                    int attempts = 0;
                    do {
                        try {
                            ++attempts;
                            peerConnectPtr->Run();
                        } catch (const std::runtime_error& e) {
                            std::lock_guard<std::mutex> cerrLock(cerrMutex);
                            std::cerr << "Runtime error: " << e.what()  << " on attempt " << attempts << std::endl;
                        } catch (const std::exception& e) {
                            std::lock_guard<std::mutex> cerrLock(cerrMutex);
                            std::cerr << "Exception: " << e.what() << std::endl;
                        } catch (...) {
                            std::lock_guard<std::mutex> cerrLock(cerrMutex);
                            std::cerr << "Unknown error" << std::endl;
                        }
                        tryAgain = peerConnectPtr->Failed() && attempts <3;
                    } while (tryAgain);
                }
        );
    }

    {
        std::lock_guard<std::mutex> coutLock(coutMutex);
        std::cout << "Started " << peerThreads.size() << " threads for peers" << std::endl;
    }

    std::this_thread::sleep_for(10s);

    {
        std::lock_guard<std::mutex> coutLock(coutMutex);
        std::cout << "Waiting to download: " << PiecesToDownload << " pieces" << std::endl;
    }

    while (pieces.PiecesSavedToDiscCount() < PiecesToDownload) {
        {
        std::lock_guard<std::mutex> coutLock(coutMutex);
        std::cout << "Already downloaded: " << pieces.PiecesSavedToDiscCount() << " pieces" << std::endl;
        }
        if (pieces.PiecesInProgressCount() == 0) {
            {
                std::lock_guard<std::mutex> coutLock(coutMutex);
                std::cout
                        << "Want to download more pieces but all peer connections are not working. Let's request new peers"
                        << std::endl;
            }

            for (auto& peerConnectPtr : peerConnections) {
                peerConnectPtr->Terminate();
            }
            for (std::thread& thread : peerThreads) {
                thread.join();
            }
            return true;
        }
        std::this_thread::sleep_for(3s);
    }

    {
        std::lock_guard<std::mutex> coutLock(coutMutex);
        std::cout << "Terminating all peer connections" << std::endl;
    }
    for (auto& peerConnectPtr : peerConnections) {
        peerConnectPtr->Terminate();
    }

    for (std::thread& thread : peerThreads) {
        thread.join();
    }
    {
        std::lock_guard<std::mutex> coutLock(coutMutex);
        std::cout << "Downloaded succesfully" << std::endl;
    }
    return false;
}

void DownloadTorrentFile(const TorrentFile& torrentFile, PieceStorage& pieces, const std::string& ourId) {
    std::cout << "Connecting to tracker " << torrentFile.announce << std::endl;
    TorrentTracker tracker(torrentFile.announce);
    bool requestMorePeers = false;
    do {
        
        tracker.UpdatePeers(torrentFile, ourId, 12345);

        if (tracker.GetPeers().empty()) {
            std::cerr << "No peers found. Cannot download a file" << std::endl;
        }

        std::cout << "Found " << tracker.GetPeers().size() << " peers" << std::endl;
        for (const Peer& peer : tracker.GetPeers()) {
            std::cout << "Found peer " << peer.ip << ":" << peer.port << std::endl;
        }

        requestMorePeers = RunDownloadMultithread(pieces, torrentFile, ourId, tracker);
    } while (requestMorePeers);
}

void TorrentClient(const std::string& file, std::string download_dir, int percent) {
    TorrentFile torrentFile;
    try {
        torrentFile = LoadTorrentFile(file);
        std::cout << "Loaded torrent file " << file << ". " << torrentFile.comment << std::endl;
    } catch (const std::invalid_argument& e) {
        std::cerr << e.what() << std::endl;
        return;
    }

    const std::filesystem::path outputDirectory = download_dir;
    PieceStorage pieces(torrentFile, outputDirectory, percent);
    PiecesToDownload = pieces.TotalPiecesCount();

    DownloadTorrentFile(torrentFile, pieces, PeerId);

    CheckDownloadedPiecesIntegrity(outputDirectory / torrentFile.name, torrentFile, pieces);
}

void parse_args(int argc, char* argv[], std::string& download_dir, int& percent, std::string& torrent_path) {
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "-d" && i + 1 < argc) {
            download_dir = argv[++i];
        } else if (arg == "-p" && i + 1 < argc) {
            percent = std::stoi(argv[++i]);
        } else if (arg.starts_with("-")) {
            std::cerr << "Unknown option: " << arg << std::endl;
            exit(1);
        } else {
            torrent_path = arg;
        }
    }

    if (download_dir.empty() || torrent_path.empty()) {
        std::cerr << "Usage: " << argv[0] << " -d <download_dir> -p <percent> <torrent_file>" << std::endl;
        exit(1);
    }
}

int main(int argc, char* argv[]) {
    std::string download_dir, torrent_path;
    int percent = 100;

    parse_args(argc, argv, download_dir, percent, torrent_path);

    TorrentClient(torrent_path, download_dir, percent);

    return 0;
}
