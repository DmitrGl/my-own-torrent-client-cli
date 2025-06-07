#include "byte_tools.h"
#include "piece.h"
#include <iostream>
#include <algorithm>

namespace {
constexpr size_t BLOCK_SIZE = 1 << 14;
}

Piece::Piece(size_t index, size_t length, std::string hash):
index_(index), length_(length),hash_(hash)
{
    
    size_t lastblocklen = length_%BLOCK_SIZE;
    blocks_.resize(length_/BLOCK_SIZE + bool(lastblocklen));
    if(lastblocklen == 0)lastblocklen = BLOCK_SIZE;
    for(size_t i = 0; i<blocks_.size();i++){
        if(i == blocks_.size() - 1){
            blocks_[i].length = lastblocklen;
        }else{
            blocks_[i].length = BLOCK_SIZE;
        }
        blocks_[i].status = Block::Status::Missing;
        blocks_[i].offset = i*BLOCK_SIZE;
        blocks_[i].piece = index_;
        blocks_[i].data = "";
    }
}

bool Piece::HashMatches() const{
    return GetDataHash() == hash_;
}

Block* Piece::FirstMissingBlock(){
    for(size_t i = 0; i<blocks_.size();i++){
        if(blocks_[i].status == Block::Status::Missing) return &blocks_[i];
    }
    throw std::runtime_error("All blocks are pending or retrieved");
}

size_t Piece::GetIndex() const{
    return index_;
}

void Piece::SaveBlock(size_t blockOffset, std::string data){
    if(blockOffset%BLOCK_SIZE != 0) throw std::runtime_error("Bad blockoffset");
    blocks_[blockOffset/BLOCK_SIZE].data = data;
    blocks_[blockOffset/BLOCK_SIZE].status = Block::Status::Retrieved;
}

bool Piece::AllBlocksRetrieved() const{
    for(size_t i = 0; i<blocks_.size();i++){
        if(blocks_[i].status != Block::Status::Retrieved) return false;
    }
    return true;
}

std::string Piece::GetData() const{
    std::string alldata = "";
    for(size_t i = 0; i<blocks_.size();i++){
        if(blocks_[i].data.empty()){
            throw std::runtime_error("Empty block exists");
        }
        alldata += blocks_[i].data;
    }
    return alldata;
}

std::string Piece::GetDataHash() const{
    std::string alldata = "";
    for(size_t i = 0; i<blocks_.size();i++){
        if(blocks_[i].data.empty()){
            throw std::runtime_error("Empty block exists");
        }
        alldata += blocks_[i].data;
    }
    return CalculateSHA1(alldata);
}

const std::string& Piece::GetHash() const{
    return hash_;
}

const size_t Piece::GetLen() const{
    return length_;
}

void Piece::Reset(){
    for(size_t i = 0; i<blocks_.size();i++){
        blocks_[i].data = "";
        blocks_[i].status = Block::Status::Missing;
    }
}