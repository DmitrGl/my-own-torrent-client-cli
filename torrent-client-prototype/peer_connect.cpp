#include "byte_tools.h"
#include "peer_connect.h"
#include "message.h"
#include <iostream>
#include <sstream>
#include <utility>
#include <cassert>
#include <thread>


using namespace std::chrono_literals;

PeerPiecesAvailability::PeerPiecesAvailability():
    bitfield_("")
{
}

PeerPiecesAvailability::PeerPiecesAvailability(std::string bitfield):
    bitfield_ (bitfield)
{
}

bool PeerPiecesAvailability::IsPieceAvailable(size_t pieceIndex) const{
     size_t byteIndex = pieceIndex / 8;
        if (byteIndex >= bitfield_.size()) {
            return false;
        }
        uint8_t byte = static_cast<uint8_t>(bitfield_[byteIndex]);
        size_t bitOffset = 7 - (pieceIndex % 8);
        return (byte & (1 << bitOffset)) != 0;
}

void PeerPiecesAvailability::SetPieceAvailability(size_t pieceIndex){
    size_t byteIndex = pieceIndex / 8;
    if (byteIndex >= bitfield_.size()) {
        size_t newSize = byteIndex + 1;
        bitfield_.resize(newSize, 0);
    }
    uint8_t& byte = reinterpret_cast<uint8_t&>(bitfield_[byteIndex]);
    size_t bitOffset = 7 - (pieceIndex % 8);
    byte |= (1 << bitOffset);
}

size_t PeerPiecesAvailability::Size() const{
    return bitfield_.size()*8;
}

PeerConnect::PeerConnect(const Peer& peer, const TorrentFile &tf, std::string selfPeerId, PieceStorage& pieceStorage):
    tf_(tf), socket_(peer.ip, peer.port, 1000ms, 3000ms), selfPeerId_(selfPeerId), terminated_(0), choked_(1),pieceInProgress_(nullptr) ,pieceStorage_(pieceStorage), pendingBlock_(0), failed_(0)
{
}

void PeerConnect::Run() {
    std::shared_lock<std::shared_mutex> lock(mtx_);
    while (!terminated_) {
        lock.unlock();
        if (EstablishConnection()) {
            //std::cout << "Connection established to peer" << std::endl;
            try{
                failed_ = 0;
                choked_ = 1;
                pendingBlock_ = 0;
                MainLoop();
            }catch(const std::exception &e){
                if(pieceInProgress_!= nullptr) pieceStorage_.RequeuePiece(pieceInProgress_);
                pieceInProgress_ = nullptr;
                failed_ = 1;
                choked_ = 1;
                pendingBlock_ = 0;
                std::string x = e.what();
                throw std::runtime_error("MainLoop error " + x); 
            }
            
        } else {
            //std::cerr << "Cannot establish connection to peer" << std::endl;
            Terminate();
        }
        lock.lock();
    }
}

void PeerConnect::PerformHandshake() {
    socket_.EstablishConnection();
    std::string handshake;
    handshake.push_back(19);
    handshake += "BitTorrent protocol";
    handshake.append(8, '\0');
    handshake += tf_.infoHash;
    handshake += selfPeerId_;
    socket_.SendData(handshake);
    std::this_thread::sleep_for(1000ms);
    int sz = int(socket_.ReceiveData(1)[0]);
    std::string protocol = socket_.ReceiveData(sz);
    if (protocol != "BitTorrent protocol") {
        throw std::runtime_error("bad protocol: " + protocol);
    }
    socket_.ReceiveData(8);

    std::string recv_info_hash = socket_.ReceiveData(20);
    if (recv_info_hash != tf_.infoHash) {
        throw std::runtime_error("Info hash mismatch");
    }
    
    peerId_ = socket_.ReceiveData(20);
}

bool PeerConnect::EstablishConnection() {
    try {
        PerformHandshake();
        ReceiveBitfield();
        SendInterested();
        return true;
    } catch (const std::exception& e) {
        //std::cerr << "Failed to establish connection with peer " << socket_.GetIp() << ":" <<
        //    socket_.GetPort() << " -- " << e.what() << std::endl;
        failed_ = 1;
        return false;
    }
}

void PeerConnect::ReceiveBitfield() {
    std::string recieved = socket_.ReceiveData();
    Message answer = Message::Parse(recieved);
    if(answer.id == MessageId::BitField){
        piecesAvailability_ = PeerPiecesAvailability(answer.payload);
    }else if(answer.id == MessageId::Unchoke){
        choked_ = false;
    }
}

void PeerConnect::SendInterested() {
    Message message = Message::Init(MessageId::Interested, "");
    std::string interested = message.ToString();
    socket_.SendData(interested);
}

void PeerConnect::RequestPiece() {
    if(pieceInProgress_ == nullptr){
        pieceInProgress_ = pieceStorage_.GetNextPieceToDownload();
        if(pieceInProgress_ == nullptr){
            Terminate();
            return;
        }
    }
    Block* neededblock = pieceInProgress_->FirstMissingBlock();
    std::string payload = toBE(pieceInProgress_->GetIndex()) + toBE(neededblock->offset) + toBE(neededblock->length);
    Message request = Message::Init(MessageId::Request, payload);
    socket_.SendData(request.ToString());
    neededblock->status = Block::Status::Pending;
    pendingBlock_ = 1;
}

void PeerConnect::Terminate() {
    std::unique_lock<std::shared_mutex> lock(mtx_);
    //std::cerr << "Terminate" << std::endl;
    terminated_ = true;
    failed_ = 0;
}

void PeerConnect::MainLoop() {
    std::shared_lock<std::shared_mutex> lock(mtx_);
    while (!terminated_) {
        lock.unlock();
        if (!choked_ && !pendingBlock_) {
            RequestPiece();
        }
        lock.lock();
        if(terminated_)return;
        lock.unlock();
        std::string recieved = socket_.ReceiveData();
        Message answer = Message::Parse(recieved);
        if(answer.id == MessageId::Have){
            //std::cout << "Recieved: Have" << std::endl;
            piecesAvailability_.SetPieceAvailability(BytesToInt(answer.payload));
        }
        if(answer.id == MessageId::Piece){
            //std::cout << "Recieved: Piece" << std::endl;
            std::string offset = answer.payload.substr(4,4);
            std::string data = answer.payload.substr(8);
            pieceInProgress_->SaveBlock(BytesToInt(offset),data);
            if(pieceInProgress_->AllBlocksRetrieved()){
                //std::cout << "AllBlocksRetrieved" << std::endl;
                pieceStorage_.PieceProcessed(pieceInProgress_);
                pieceInProgress_ = nullptr;
            }
            pendingBlock_ = 0;
        }
        if(answer.id == MessageId::Choke){
            //std::cout << "Recieved: Choke" << std::endl;
            choked_ = 1;
        }
        if(answer.id == MessageId::Unchoke){
            //std::cout << "Recieved: Unchoke" << std::endl;
            choked_ = 0;
        }
        lock.lock();
    }
}

bool PeerConnect::Failed() const {
    return failed_;
}
