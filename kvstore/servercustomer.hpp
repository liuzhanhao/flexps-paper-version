#pragma once

#include <condition_variable>
#include <functional>
#include <mutex>
#include <thread>
#include "base/serialization.hpp"
#include "core/mailbox.hpp"

namespace kvstore {

/*
 * ServerCustomer is only for KVManager!!!
 */
class ServerCustomer {
   public:
    /*
     * the handle for a received message
     */
    using RecvHandle = std::function<void(int, int, husky::base::BinStream&)>;

    ServerCustomer(husky::LocalMailbox& mailbox, const RecvHandle& recv_handle, int channel_id)
        : mailbox_(mailbox), recv_handle_(recv_handle), channel_id_(channel_id) {}
    ~ServerCustomer() { recv_thread_->join(); }
    void Start() {
        // spawn a new thread to recevive
        recv_thread_ = std::unique_ptr<std::thread>(new std::thread(&ServerCustomer::Receiving, this));
    }
    void Stop() {
        husky::base::BinStream bin;  // send an empty BinStream
        mailbox_.send(mailbox_.get_thread_id(), channel_id_, 0, bin);
    }
    void send(int dst, husky::base::BinStream& bin) { mailbox_.send(dst, channel_id_, 0, bin); }

   private:
    void Receiving() {
        // poll and recv from mailbox
        int num_finished_workers = 0;
        while (mailbox_.poll(channel_id_, 0)) {
            auto bin = mailbox_.recv(channel_id_, 0);
            if (bin.size() == 0) {
                break;
            }
            // Format: isRequest, kv_id, ts, push, src, k, v...
            // response: 0, kv_id, ts, push, src, keys, vals ; handled by worker
            // request: 1, kv_id, ts, push, src, k, v, k, v... ; handled by server
            bool isRequest;
            int kv_id;
            int ts;
            bin >> isRequest >> kv_id >> ts;
            // invoke the callback
            recv_handle_(kv_id, ts, bin);
        }
    }

    // mailbox
    husky::LocalMailbox& mailbox_;  // reference to mailbox

    // receive thread and receive handle
    RecvHandle recv_handle_;
    std::unique_ptr<std::thread> recv_thread_;

    // some info
    int channel_id_;
};

}  // namespace kvstore