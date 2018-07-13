#pragma once
#include "ptcp_conn.h"
#include "spsc_varq.h"
#include "mmap.h"


template<class Conf>
class TcpShmConnection
{
public:
    TcpShmConnection() {
        remote_name_[0] = 0;
    }

    void init(const char* ptcp_dir, const char* local_name) {
        ptcp_dir_ = ptcp_dir;
        local_name_ = local_name;
    }

    bool OpenFile(bool use_shm,
                  const char** error_msg) {
        if(use_shm) {
            std::string shm_send_file = std::string("/") + local_name_ + "_" + remote_name_ + ".shm";
            std::string shm_recv_file = std::string("/") + remote_name_ + "_" + local_name_ + ".shm";
            if(!shm_sendq_) {
                shm_sendq_ = my_mmap<SHMQ>(shm_send_file.c_str(), true, error_msg);
                if(!shm_sendq_) return false;
            }
            if(!shm_recvq_) {
                shm_recvq_ = my_mmap<SHMQ>(shm_recv_file.c_str(), true, error_msg);
                if(!shm_recvq_) return false;
            }
            return true;
        }
        std::string ptcp_send_file = GetPtcpFile();
        return ptcp_conn_.OpenFile(ptcp_send_file.c_str(), error_msg);
    }
    std::string GetPtcpFile() {
        return std::string(ptcp_dir_) + "/" + local_name_ + "_" + remote_name_ + ".ptcp";
    }

    bool GetSeq(uint32_t* local_ack_seq, uint32_t* local_seq_start, uint32_t* local_seq_end, const char** error_msg) {
        if(shm_sendq_) return true;
        if(!ptcp_conn_.GetSeq(local_ack_seq, local_seq_start, local_seq_end)) {
            *error_msg = "Ptcp file corrupt";
            errno = 0;
            return false;
        }
        return true;
    }

    void Reset() {
        if(shm_sendq_) {
            memset(shm_sendq_, 0, sizeof(SHMQ));
            memset(shm_recvq_, 0, sizeof(SHMQ));
        }
        else {
            ptcp_conn_.Reset();
        }
    }

    void Release() {
        remote_name_[0] = 0;
        if(shm_sendq_) {
            my_munmap<SHMQ>(shm_sendq_);
            shm_sendq_ = nullptr;
        }
        if(shm_recvq_) {
            my_munmap<SHMQ>(shm_recvq_);
            shm_recvq_ = nullptr;
        }
        ptcp_conn_.Release();
    }

    void Open(int sock_fd, uint32_t remote_ack_seq, int64_t now) {
        ptcp_conn_.Open(sock_fd, remote_ack_seq, now);
    }

    bool IsClosed() {
        return ptcp_conn_.IsClosed();
    }

    void RequestClose() {
        ptcp_conn_.RequestClose();
    }

    const char* GetCloseReason(int& sys_errno) {
        return ptcp_conn_.GetCloseReason(sys_errno);
    }

    char* GetRemoteName() {
        return remote_name_;
    }

    const char* GetLocalName() {
        return local_name_;
    }

    const char* GetPtcpDir() {
        return ptcp_dir_;
    }

    MsgHeader* Alloc(uint16_t size) {
        if(shm_sendq_) {
            return shm_sendq_->Alloc(size);
        }
        return ptcp_conn_.Alloc(size);
    }

    void Push() {
        if(shm_sendq_) {
            return shm_sendq_->Push();
        }
        return ptcp_conn_.Push();
    }

    MsgHeader* TcpFront(int64_t now) {
        ptcp_conn_.SendHB(now);
        return ptcp_conn_.Front(); // for shm, we need to recv HB and Front() always return nullptr
    }

    MsgHeader* ShmFront() {
        return shm_recvq_->Front();
    }

    void Pop() {
        if(shm_recvq_) {
            shm_recvq_->Pop();
        }
        else {
            ptcp_conn_.Pop();
        }
    }

    void PushAndPop() {
        if(shm_sendq_) {
            shm_sendq_->Push();
            shm_recvq_->Pop();
        }
        else {
            ptcp_conn_.PushAndPop();
        }
    }

public:
    typename Conf::ConnectionUserData user_data;

private:
    const char* local_name_;
    char remote_name_[Conf::NameSize];
    const char* ptcp_dir_ = nullptr;
    PTCPConnection<Conf> ptcp_conn_;
    typedef SPSCVarQueue<Conf::ShmQueueSize> SHMQ;
    alignas(64) SHMQ* shm_sendq_ = nullptr;
    SHMQ* shm_recvq_ = nullptr;
};
