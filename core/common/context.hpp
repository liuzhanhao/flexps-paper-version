#pragma once

#include <string>
#include "zmq.hpp"
#include "core/common/config.hpp"
#include "core/common/worker_info.hpp"

namespace husky {

struct Global {
    Config config;
    zmq::context_t* zmq_context_ptr = nullptr;
    WorkerInfo worker_info;
};

class Context {
public:
    static void init_global() {
        global = new Global();
        global->zmq_context_ptr = new zmq::context_t();
    }
    static void finalize_global() {
        auto p = global->zmq_context_ptr;
        delete global;
        delete p;
        global = nullptr;
    }

    static Global* get_global()  { return global; }
    static Config* get_config() { return &(global->config); }
    static std::string get_param(const std::string& key) { return global->config.get_param(key); }
    static auto& get_params() { return global->config.get_params(); }
    static zmq::context_t& get_zmq_context() { return *(global->zmq_context_ptr); }
    static WorkerInfo* get_worker_info() { return &(global->worker_info); }

private:
    static Global* global;
};

}  // namespace husky