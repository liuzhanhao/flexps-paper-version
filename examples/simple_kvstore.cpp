#include "kvstore/kvstore.hpp"
#include "worker/engine.hpp"

#include "core/color.hpp"
#include "kvstore/consistency/bsp.hpp"

using namespace husky;

int main(int argc, char** argv) {
    bool rt = init_with_args(argc, argv, {"worker_port", "cluster_manager_host", "cluster_manager_port"});
    if (!rt)
        return 1;

    auto& engine = Engine::Get();
    // Start the kvstore, should start after mailbox is up
    kvstore::KVStore::Get().Start(Context::get_worker_info(), Context::get_mailbox_event_loop(),
                                  Context::get_zmq_context());

    auto task = TaskFactory::Get().CreateTask<Task>(1, 1);
    int kv1 = kvstore::KVStore::Get().CreateKVStore<float>();
    engine.AddTask(task, [kv1](const Info& info) {
        auto* kvworker = kvstore::KVStore::Get().get_kvworker(info.get_local_id());
        std::vector<int> keys{0};
        std::vector<float> vals{2.0};
        int ts = kvworker->PushLocal(kv1, info.get_proc_id(), keys, vals);
        // int ts = kvworker->Push(kv1, keys, vals);
        kvworker->Wait(kv1, ts);
        husky::LOG_I << "Push Done!";

        std::vector<float> rets;
        kvworker->Wait(kv1, kvworker->PullLocal(kv1, info.get_proc_id(), keys, &rets));
        // kvworker->Wait(kv1, kvworker->Pull(kv1, keys, &rets));
        husky::LOG_I << rets[0];
    });
    engine.Submit();

    task = TaskFactory::Get().CreateTask<Task>(1, 4);
    // int kv2 = kvstore::KVStore::Get().CreateKVStore<float>(kvstore::KVServerDefaultAddHandle<float>());
    int kv2 = kvstore::KVStore::Get().CreateKVStore<float>(kvstore::KVServerBSPHandle<float>(4));
    engine.AddTask(task, [kv2](const Info& info) {
        auto* kvworker = kvstore::KVStore::Get().get_kvworker(info.get_local_id());
        kvworker->Push(kv2, {0}, std::vector<float>{0.0});
        for (int i = 0; i < 50; ++ i) {
            std::vector<int> keys{0};
            // pull
            std::vector<float> rets;
            kvworker->Wait(kv2, kvworker->Pull(kv2, keys, &rets));  // in BSP, expect to see all the update
            husky::LOG_I << BLUE("id:"+std::to_string(info.get_local_id())+" iter "+std::to_string(i)+": "+std::to_string(rets[0]));

            // push
            std::vector<float> vals{2.0};
            kvworker->Push(kv2, keys, vals);
        }
    });
    engine.Submit();

    engine.Exit();
    // Stop the kvstore, should stop before mailbox is down
    kvstore::KVStore::Get().Stop();
}
