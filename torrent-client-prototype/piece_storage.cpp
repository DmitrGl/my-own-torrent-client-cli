#include "piece_storage.h"
#include <iostream>


PieceStorage::PieceStorage(const TorrentFile& tf, const std::filesystem::path& outputDirectory, int percent){
    std::filesystem::path filePath = outputDirectory / tf.name;

    outfile_.open(filePath, std::ios::binary | std::ios::out);
    if (!outfile_.is_open()) {
        throw std::runtime_error("Cannot create file: " + filePath.string());
    }

    outfile_.seekp(tf.length - 1);
    outfile_.write("\0", 1);
    outfile_.flush(); 
    PiecesCount_ = tf.pieceHashes.size()*(percent/100.0);
    for(size_t i = 0; i < PiecesCount_; i++){
        PiecePtr nwpc = std::make_shared<Piece>(i, tf.pieceLength, tf.pieceHashes[i]);
        remainPieces_.push(nwpc);
    }
}

PiecePtr PieceStorage::GetNextPieceToDownload() {
    std::lock_guard<std::mutex> lock(mtx_);
    if(remainPieces_.empty())return nullptr;
    PiecePtr ans = remainPieces_.front();
    remainPieces_.pop();
    ProgressCnt_++;
    return ans;
}

void PieceStorage::PieceProcessed(const PiecePtr& piece) {
    std::lock_guard<std::mutex> lock(mtx_);
    ProgressCnt_--;
    if(piece->AllBlocksRetrieved() && piece->HashMatches()){
        SavePieceToDisk(piece);
        saved_.push_back(piece->GetIndex());
    }else{
        throw std::runtime_error("Piece hash mismatch");
    }
}

bool PieceStorage::QueueIsEmpty() const {
    std::lock_guard<std::mutex> lock(mtx_);
    return remainPieces_.empty();
}

size_t PieceStorage::PiecesSavedToDiscCount() const{
    std::lock_guard<std::mutex> lock(mtx_);
    return saved_.size(); 
}

size_t PieceStorage::TotalPiecesCount() const {
    std::lock_guard<std::mutex> lock(mtx_);
    return PiecesCount_;
}

void PieceStorage::CloseOutputFile(){
    std::lock_guard<std::mutex> lock(mtx_);
    outfile_.close();
}

const std::vector<size_t>& PieceStorage::GetPiecesSavedToDiscIndices() const{
    std::lock_guard<std::mutex> lock(mtx_);
    return saved_;
}

size_t PieceStorage::PiecesInProgressCount() const{
    std::lock_guard<std::mutex> lock(mtx_);
    return ProgressCnt_;
}

void PieceStorage::SavePieceToDisk(const PiecePtr& piece){
    outfile_.seekp(piece->GetLen()*piece->GetIndex());
    outfile_.write(piece->GetData().data(), piece->GetLen());
    outfile_.flush();
}

void PieceStorage::RequeuePiece(const PiecePtr& piece){
    std::lock_guard<std::mutex> lock(mtx_);
    ProgressCnt_--;
    piece->Reset();
    remainPieces_.push(piece);
}