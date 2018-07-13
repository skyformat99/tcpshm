#pragma once
#include "msg_header.h"
#include <bits/stdc++.h>

// Simple single thread persist Queue that can be mmap-ed to a file
template<uint32_t Bytes>
class PTCPQueue
{
public:
    static_assert(Bytes % sizeof(MsgHeader) == 0, "Bytes must be multiple of 8");
    static const uint32_t BLK_CNT = Bytes / sizeof(MsgHeader);

    MsgHeader* Alloc(uint16_t size) {
        size += sizeof(MsgHeader);
        uint32_t blk_sz = (size + sizeof(MsgHeader) - 1) / sizeof(MsgHeader);
        uint32_t avail_sz = BLK_CNT - write_idx_;
        if(blk_sz > avail_sz) {
            if(blk_sz > avail_sz + read_idx_) return nullptr;
            memmove(blk_, blk_ + read_idx_, (write_idx_ - read_idx_) * sizeof(MsgHeader));
            write_idx_ -= read_idx_;
            send_idx_ -= read_idx_;
            read_idx_ = 0;
        }
        MsgHeader& header = blk_[write_idx_];
        header.size = size;
        /*
        std::cout << "Alloc read_idx_: " << read_idx_ << " send_idx_: " << send_idx_ << " write_idx_: " << write_idx_
                  << std::endl;
                  */
        return &header;
    }

    void Push() {
        MsgHeader& header = blk_[write_idx_];
        uint32_t blk_sz = (header.size + sizeof(MsgHeader) - 1) / sizeof(MsgHeader);
        header.ack_seq = ack_seq_num_;
        write_idx_ += blk_sz;
        /*
        std::cout << "Push read_idx_: " << read_idx_ << " send_idx_: " << send_idx_ << " write_idx_: " << write_idx_
                  << std::endl;
                  */
    }

    MsgHeader* GetSendable(int& blk_sz) {
        blk_sz = write_idx_ - send_idx_;
        return blk_ + send_idx_;
    }

    void Sendout(int blk_sz) {
        send_idx_ += blk_sz;
    }

    void LoginAck(uint32_t ack_seq) {
        Ack(ack_seq);
        send_idx_ = read_idx_;
    }

    // the next seq_num peer side expect
    void Ack(uint32_t ack_seq) {
        if((int)(ack_seq - read_seq_num_) <= 0) return; // if ack_seq is not newer than read_seq_num_
        // we assume that a successfuly logined client will not attack us
        // so_seq will never go beyond the msg write_idx_ points to during a connection lifecycle
        do {
            read_idx_ += (blk_[read_idx_].size + sizeof(MsgHeader) - 1) / sizeof(MsgHeader);
            read_seq_num_++;
        } while(read_seq_num_ != ack_seq);
        if(read_idx_ == write_idx_) {
            read_idx_ = write_idx_ = send_idx_ = 0;
        }
    }

    uint32_t& MyAck() {
        return ack_seq_num_;
    }

    bool SanityCheckAndGetSeq(uint32_t* seq_start, uint32_t* seq_end) {
        uint32_t end = read_seq_num_;
        uint32_t idx = read_idx_;
        while(idx < write_idx_) {
            if((int)(ack_seq_num_ - blk_[idx].ack_seq) < 0) return false; // ack_seq in this msg is too new
            idx += (blk_[idx].size + sizeof(MsgHeader) - 1) / sizeof(MsgHeader);
            end++;
        }
        if(idx != write_idx_) return false;
        *seq_start = read_seq_num_;
        *seq_end = end;
        return true;
    }

    void Print() {
        std::cout << "Print read_idx_: " << read_idx_ << " send_idx_: " << send_idx_ << " write_idx_: " << write_idx_
                  << " read_seq_num_: " << read_seq_num_ << " ack_seq_num_: " << ack_seq_num_ << std::endl;
    }

private:
    MsgHeader blk_[BLK_CNT];
    // invariant: read_idx_ <= send_idx_ <= write_idx_
    // where send_idx_ may point to the middle of a msg
    uint32_t write_idx_;
    uint32_t read_idx_;
    uint32_t send_idx_;
    uint32_t read_seq_num_; // the seq_num_ of msg read_idx_ points to
    uint32_t ack_seq_num_;
};
