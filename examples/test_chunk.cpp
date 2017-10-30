#include <vector>

#include "worker/engine.hpp"
#include "ml/ml.hpp"

#include "core/color.hpp"

using namespace husky;

int main(int argc, char** argv) {
    bool rt = init_with_args(argc, argv, {"worker_port", "cluster_manager_host", "cluster_manager_port",
        "dims", "chunk_size", "threads_per_process", "iters",
        "staleness", "worker_type"});
    if (!rt)
        return 1;

    auto& engine = Engine::Get();
    kvstore::KVStore::Get().Start(Context::get_worker_info(), Context::get_mailbox_event_loop(),
                                  Context::get_zmq_context());
    const int dims = std::stoi(Context::get_param("dims"));
    const int chunk_size = std::stoi(Context::get_param("chunk_size"));
    assert(dims % chunk_size == 0);
    const int staleness = std::stoi(Context::get_param("staleness"));
    const int kv = kvstore::KVStore::Get().CreateKVStore<float>("ssp_add_vector", 1, 0, 
        dims, chunk_size);
    const int threads_per_process = std::stoi(Context::get_param("threads_per_process"));
    const int iters = std::stoi(Context::get_param("iters"));

    auto train_task = TaskFactory::Get().CreateTask<ConfigurableWorkersTask>();
    train_task.set_total_epoch(1);
    train_task.set_worker_num({threads_per_process});
    train_task.set_worker_num_type({"threads_per_worker"});
    TableInfo table_info {
        kv, dims,
        husky::ModeType::PS, 
        husky::Consistency::SSP, 
        // husky::WorkerType::PSNoneChunkWorker, 
        husky::WorkerType::PSWorker, 
        husky::ParamType::None,
        staleness
    };
    const std::string worker_type = Context::get_param("worker_type");
    if (worker_type == "PSNoneChunkWorker") {
      table_info.worker_type = husky::WorkerType::PSNoneChunkWorker;
    } else if (worker_type == "PSWorker") {
      table_info.worker_type = husky::WorkerType::PSWorker;
    } else {
      husky::LOG_I << "Unknown " << worker_type;
      assert(false);
      return 1;
    }

    engine.AddTask(train_task, [table_info, iters, dims, chunk_size](const Info& info) {
        int num_chunks = dims / chunk_size;
        std::vector<size_t> chunk_ids(num_chunks);
        std::iota(chunk_ids.begin(), chunk_ids.end(), 0);
        std::vector<std::vector<float>> params(num_chunks);
        // std::vector<std::vector<float>> step_sums(num_chunks);
        std::vector<std::vector<float>*> pull_ptrs(num_chunks);
        std::vector<std::vector<float>*> push_ptrs(num_chunks);
        for (int i = 0; i < num_chunks; ++i) {
            params[i].resize(chunk_size);
            pull_ptrs[i] = &params[i];
            push_ptrs[i] = &params[i];
        }

        auto worker = ml::CreateMLWorker<float>(info, table_info);
        std::chrono::microseconds pull_time{0};
        std::chrono::microseconds push_time{0};
        auto start_time = std::chrono::steady_clock::now();
        for (int iter = 0; iter < iters; ++ iter) {
            auto t1 = std::chrono::steady_clock::now();
            worker->PullChunks(chunk_ids, pull_ptrs);
            auto t2 = std::chrono::steady_clock::now();
            worker->PushChunks(chunk_ids, push_ptrs);
            auto t3 = std::chrono::steady_clock::now();

            pull_time += std::chrono::duration_cast<std::chrono::microseconds>(t2-t1);
            push_time += std::chrono::duration_cast<std::chrono::microseconds>(t3-t3);
            if (info.get_cluster_id() == 0) {
                auto iter_time = std::chrono::duration_cast<std::chrono::microseconds>(t3-t1);
                husky::LOG_I << "iter_time: " << iter_time.count()/1000.0 << " ms";
            }
        }
        auto end_time = std::chrono::steady_clock::now();
        auto total_time = std::chrono::duration_cast<std::chrono::milliseconds>(end_time-start_time);
        husky::LOG_I << "pull_time:" << pull_time.count()/1000.0 << " ms, "
          << "push_time:" << push_time.count()/1000.0 << " ms, "
          << "total_time: " << total_time.count() << " ms";
    });
    engine.Submit();
    engine.Exit();
    kvstore::KVStore::Get().Stop();
}
