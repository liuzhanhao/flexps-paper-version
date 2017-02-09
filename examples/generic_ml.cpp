#include <vector>

#include "ml/common/mlworker.hpp"
#include "worker/engine.hpp"

#include "core/color.hpp"

using namespace husky;

auto test_mlworker_lambda = [](const Info& info) {
    auto& worker = info.get_mlworker();
    int num_iter = 1001;
    for (int i = 0; i < num_iter; ++ i) {
        std::vector<float> rets;
        std::vector<husky::constants::Key> keys{0};
        // pull
        worker->Pull(keys, &rets);
        if (i % 100 == 0)
            husky::LOG_I << BLUE("id:" + std::to_string(info.get_local_id()) + " iter " + std::to_string(i) + ": " +
                                 std::to_string(rets[0]));
        // push
        std::vector<float> vals{1.0};
        worker->Push(keys, vals);
    }
};

int main(int argc, char** argv) {
    bool rt = init_with_args(argc, argv, {"worker_port", "cluster_manager_host", "cluster_manager_port"});
    if (!rt)
        return 1;

    auto& engine = Engine::Get();
    // Start the kvstore, should start after mailbox is up
    kvstore::KVStore::Get().Start(Context::get_worker_info(), Context::get_mailbox_event_loop(),
                                  Context::get_zmq_context());
    // Didn't specify the epoch num and thread num, leave cluster_manager to decide them

    //  A Hogwild! Task
    int kv1 = kvstore::KVStore::Get().CreateKVStore<float>();
    auto task1 = TaskFactory::Get().CreateTask<MLTask>();
    task1.set_dimensions(10);
    task1.set_kvstore(kv1);
    task1.set_hint("hogwild");  // set the running type explicitly
    task1.set_total_epoch(2);                             // 2 epochs
    task1.set_num_workers(4);                             // 4 workers
    engine.AddTask(task1, [](const Info& info) {
        auto& worker = info.get_mlworker();
        // int k = 3;
        // worker->Put(k, 0.456);
        // float v = worker->Get(k);
        // base::log_msg("k: " + std::to_string(k) + " v: " + std::to_string(v));
        int start = info.get_cluster_id();
        for (int i = 0; i < 10000; ++i) {
            worker->Put(start, 0.01);
            start += 1;
            start %= static_cast<MLTask*>(info.get_task())->get_dimensions();
        }
    });

    // A Single Task
    int kv2 = kvstore::KVStore::Get().CreateKVStore<float>();
    auto task2 = TaskFactory::Get().CreateTask<MLTask>();
    task2.set_dimensions(5);
    task2.set_kvstore(kv2);
    task2.set_hint("single");  // set the running type explicitly
    engine.AddTask(task2, [](const Info& info) {
        auto& worker = info.get_mlworker();
        // int k = 3;
        // worker->Put(k, 0.456);
        // float v = worker->Get(k);
        // base::log_msg("k: " + std::to_string(k) + " v: " + std::to_string(v));
        int start = info.get_cluster_id();
        for (int i = 0; i < 10000; ++i) {
            worker->Put(start, 0.01);
            start += 1;
            start %= static_cast<MLTask*>(info.get_task())->get_dimensions();
        }
    });

    // A PS Task
    // int kv3 = kvstore::KVStore::Get().CreateKVStore<float>(kvstore::KVServerDefaultAddHandle<float>());
    // BSP
    int kv3 =
        kvstore::KVStore::Get().CreateKVStore<float>("BSP:4");  // for bsp server
    auto task3 = TaskFactory::Get().CreateTask<MLTask>();
    task3.set_dimensions(5);
    task3.set_kvstore(kv3);
    task3.set_hint("PS:BSP");  // set the running type explicitly
    task3.set_num_workers(4);                           // 4 workers
    engine.AddTask(task3, [](const Info& info) {
        if (info.get_cluster_id() == 0)
            husky::LOG_I << "PS BSP Model running";
        test_mlworker_lambda(info);
    });

    // SSP
    int kv4 = kvstore::KVStore::Get().CreateKVStore<float>("SSP:4:1");  // staleness: 1
    auto task4 = TaskFactory::Get().CreateTask<MLTask>();
    task4.set_dimensions(5);
    task4.set_kvstore(kv4);
    task4.set_hint("PS:SSP");  // set the running type explicitly
    task4.set_num_workers(4);                           // 4 workers
    engine.AddTask(task4, [](const Info& info) {
        if (info.get_cluster_id() == 0)
            husky::LOG_I << "PS SSP Model running";
        test_mlworker_lambda(info);
    });

    // ASP
    int kv5 = kvstore::KVStore::Get().CreateKVStore<float>("Add");  // use the default add handle
    auto task5 = TaskFactory::Get().CreateTask<MLTask>();
    task5.set_dimensions(5);
    task5.set_kvstore(kv5);
    task5.set_hint("PS:ASP");  // set the running type explicitly
    task5.set_num_workers(4);                           // 4 workers
    engine.AddTask(task5, [](const Info& info) {
        if (info.get_cluster_id() == 0)
            husky::LOG_I << "PS ASP Model running";
        test_mlworker_lambda(info);
    });

    //  A SPMT Task
    int kv6 = kvstore::KVStore::Get().CreateKVStore<float>();
    auto task6 = TaskFactory::Get().CreateTask<MLTask>();
    task6.set_dimensions(10);
    task6.set_kvstore(kv6);
    task6.set_hint("SPMT:ASP");  // set the running type explicitly
    task6.set_num_workers(4);                             // 4 workers
    engine.AddTask(task6, [](const Info& info) {
        test_mlworker_lambda(info);
    });

    engine.Submit();
    engine.Exit();
    // Stop the kvstore, should stop before mailbox is down
    kvstore::KVStore::Get().Stop();
}
