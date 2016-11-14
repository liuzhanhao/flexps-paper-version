#pragma once

#include "base/debug.hpp"
#include "base/log.hpp"

#include "zmq.hpp" 
#include "base/serialization.hpp"
#include "core/common/constants.hpp"
#include "core/common/instance.hpp"
#include "core/worker/instance_runner.hpp"
#include "core/common/worker_info.hpp"
#include "core/common/zmq_helpers.hpp"
#include "core/worker/master_connector.hpp"
#include "core/worker/task_store.hpp"

namespace husky {

class Worker {
public:
    Worker() = delete;
    Worker(WorkerInfo&& worker_info_, MasterConnector&& master_connector_)
        : worker_info(std::move(worker_info_)),
        master_connector(std::move(master_connector_)),
        instance_runner(worker_info, master_connector, task_store){
    }

    // User need to add task to taskstore
    void add_task(const Task& task, std::function<void(Info)> func) {
        task_store.add_task(task, func);
    }

    void send_tasks_to_master() {
        // Only Proc 0 need to send tasks to master
        if (worker_info.get_proc_id() == 0) {
            base::BinStream bin;
            auto& task_map = task_store.get_task_map();
            bin << task_map.size();
            for (auto& kv : task_map) {
                Task task = kv.second.first;  // push the Task to buffer
                bin << task;
            }
            auto& socket = master_connector.get_send_socket();
            zmq_send_binstream(&socket, bin);
            base::log_msg("[Worker]: Totally "+std::to_string(task_map.size())+" tasks sent");
        }
    }

    void main_loop() {
        auto& socket = master_connector.get_recv_socket();
        auto& send_socket = master_connector.get_send_socket();
        while (true) {
            int type = zmq_recv_int32(&socket);
            base::log_msg("[Worker]: " + std::to_string(type) );
            if (type == constants::TASK_TYPE) {
                auto bin = zmq_recv_binstream(&socket);
                Instance instance;
                bin >> instance;
                instance.show_instance();
                instance_runner.run_instance(instance);
            }
            else if (type == constants::THREAD_FINISHED) {
                int instance_id = zmq_recv_int32(&socket);
                int thread_id = zmq_recv_int32(&socket);
                instance_runner.finish_thread(instance_id, thread_id);
                bool is_instance_done = instance_runner.is_instance_done(instance_id);
                if (is_instance_done) {
                    instance_runner.remove_instance(instance_id);
                }
            }
            else if (type == constants::MASTER_FINISHED) {
                base::log_msg("[Worker]: worker exit");
                break;
            }
        }
    }

private:
    MasterConnector master_connector;
    WorkerInfo worker_info;
    InstanceRunner instance_runner;
    TaskStore task_store;
};

}  // namespace husky
