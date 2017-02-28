#pragma once

#include <unordered_map>

#include "husky/base/serialization.hpp"
#include "kvstore/handles/basic.hpp"
#include "kvstore/kvmanager.hpp"
#include "kvstore/ps_lite/sarray.h"

#include "core/color.hpp"

#include "kvstore/servercustomer.hpp"

namespace kvstore {

class ServerBase {
   public:
    virtual void Process(int kv_id, int ts, husky::base::BinStream& bin, ServerCustomer* customer) = 0;
    /*
     * response to the push/pull request
     * The whole callback process is:
     * process -> HandleAndReply -> Response
     */
    template<typename Val>
    void Response(int kv_id, int ts, int cmd, bool push, int src, const KVPairs<Val>& res, ServerCustomer* customer) {
        husky::base::BinStream bin;
        bool isRequest = false;
        // isRequest, kv_id, ts, isPush, src
        bin << isRequest << kv_id << ts << cmd << push << src;
        if (cmd == 2 && push == false) {  // enable zero-copy for local Pull
            KVPairs<Val>* p = new KVPairs<Val>();  // delete by worker
            p->keys = res.keys;
            p->vals = res.vals;
            bin << reinterpret_cast<std::uintptr_t>(p);
        } else {
            bin << res.keys << res.vals;
        }
        customer->send(src, bin);
    }
};

/*
 * The default functor for assign operation
 */
template <typename Val>
class DefaultAddServer : public ServerBase {
   public:
    DefaultAddServer() = delete;
    DefaultAddServer(int kv_id, int server_id) : server_id_(server_id) {}

    virtual void Process(int kv_id, int ts, husky::base::BinStream& bin, ServerCustomer* customer) override {
        int cmd;
        bool push;  // push or not
        int src;
        bin >> cmd >> push >> src;
        if (push == true) {  // if is push
            if (bin.size()) {  // if bin is empty, don't reply
                update(kv_id, bin, store_, cmd);
                Response<Val>(kv_id, ts, cmd, push, src, KVPairs<Val>(), customer);
            }
        } else {  // if is pull
            if (bin.size()) {  // if bin is empty, don't reply
                KVPairs<Val> res = retrieve(kv_id, bin, store_, cmd);
                Response<Val>(kv_id, ts, cmd, push, src, res, customer);
            }
        }
    }
   private:
    // The real storeage
    std::unordered_map<husky::constants::Key, Val> store_;
    int server_id_;
};

/*
 * The vector functor for assign operation
 */
template <typename Val>
class VectorAddServer : public ServerBase {
   public:
    VectorAddServer() = delete;
    VectorAddServer(int kv_id, int server_id) : server_id_(server_id) {
        store_.resize(RangeManager::Get().GetServerSize(kv_id, server_id_));
    }

    virtual void Process(int kv_id, int ts, husky::base::BinStream& bin, ServerCustomer* customer) override {
        int cmd;
        bool push;  // push or not
        int src;
        bin >> cmd >> push >> src;
        if (push == true) {  // if is push
            if (bin.size()) {  // if bin is empty, don't reply
                update(kv_id, bin, store_, cmd, server_id_);
                Response<Val>(kv_id, ts, cmd, push, src, KVPairs<Val>(), customer);
            }
        } else {  // if is pull
            if (bin.size()) {  // if bin is empty, don't reply
                KVPairs<Val> res = retrieve(kv_id, bin, store_, cmd, server_id_);
                Response<Val>(kv_id, ts, cmd, push, src, res, customer);
            }
        }
    }
   private:
    // The real storeage
    std::vector<Val> store_;
    int server_id_;
};

/*
 * The default functor for assign operation
 */
template <typename Val>
class DefaultAssignServer : public ServerBase {
   public:
    DefaultAssignServer() = delete;
    DefaultAssignServer(int kv_id, int server_id) : server_id_(server_id) {}

    virtual void Process(int kv_id, int ts, husky::base::BinStream& bin, ServerCustomer* customer) override {
        int cmd;
        bool push;  // push or not
        int src;
        bin >> cmd >> push >> src;
        if (push == true) {  // if is push
            if (bin.size()) {  // if bin is empty, don't reply
                assign(kv_id, bin, store_, cmd);
                Response<Val>(kv_id, ts, cmd, push, src, KVPairs<Val>(), customer);
            }
        } else {  // if is pull
            if (bin.size()) {  // if bin is empty, don't reply
                KVPairs<Val> res = retrieve(kv_id, bin, store_, cmd);
                Response<Val>(kv_id, ts, cmd, push, src, res, customer);
            }
        }
    }
   private:
    // The real storeage
    std::unordered_map<husky::constants::Key, Val> store_;
    int server_id_;
};

/*
 * The vector functor for assign operation
 */
template <typename Val>
class VectorAssignServer : public ServerBase {
   public:
    VectorAssignServer() = delete;
    VectorAssignServer(int kv_id, int server_id) : server_id_(server_id) {
        store_.resize(RangeManager::Get().GetServerSize(kv_id, server_id_));
    }

    virtual void Process(int kv_id, int ts, husky::base::BinStream& bin, ServerCustomer* customer) override {
        int cmd;
        bool push;  // push or not
        int src;
        bin >> cmd >> push >> src;
        if (push == true) {  // if is push
            if (bin.size()) {  // if bin is empty, don't reply
                assign(kv_id, bin, store_, cmd, server_id_);
                Response<Val>(kv_id, ts, cmd, push, src, KVPairs<Val>(), customer);
            }
        } else {  // if is pull
            if (bin.size()) {  // if bin is empty, don't reply
                KVPairs<Val> res = retrieve(kv_id, bin, store_, cmd, server_id_);
                Response<Val>(kv_id, ts, cmd, push, src, res, customer);
            }
        }
    }
   private:
    // The real storeage
    std::vector<Val> store_;
    int server_id_;
};

}  // namespace kvstore
