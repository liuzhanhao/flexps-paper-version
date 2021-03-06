#include "kvstore/kvstore.hpp"
#include "worker/engine.hpp"

#include "core/color.hpp"

#include "ml/model/load.hpp"
#include "ml/model/dump.hpp"

using namespace husky;

int main(int argc, char** argv) {
    bool rt = init_with_args(argc, argv, {"worker_port", "cluster_manager_host", "cluster_manager_port"});
    if (!rt)
        return 1;

    auto& engine = Engine::Get();
    // Start the kvstore, should start after mailbox is up
    kvstore::KVStore::Get().Start(Context::get_worker_info(), Context::get_mailbox_event_loop(),
                                  Context::get_zmq_context(), 2);
    auto task = TaskFactory::Get().CreateTask<Task>(1, 1);
    int num_params = 100;
    int chunk_size = 10;
    int kv1 = kvstore::KVStore::Get().CreateKVStore<float>({}, num_params, chunk_size);
    engine.AddTask(task, [kv1, num_params, chunk_size](const Info& info) {
        {
            // Test LoadIntegralFromKV, DumpIntegralToKV
            std::vector<float> params;
            ml::model::LoadIntegralFromKV(info.get_local_id(), kv1, num_params, &params);
            for (int i = 0; i < params.size(); ++ i) {
                assert(params[i] == 0);
                params[i] = 1;
            }
            ml::model::DumpIntegralToKV(info.get_local_id(), kv1, num_params, params);
            ml::model::LoadIntegralFromKV(info.get_local_id(), kv1, num_params, &params);
            for (int i = 0; i < params.size(); ++ i) {
                assert(params[i] == 1);
            }
        }

        {
            // Test LoadAllChunksFromKV/DumpAllChunksToKV
            std::vector<std::vector<float>> chunks(chunk_size);
            ml::model::LoadAllChunksFromKV(info.get_local_id(), kv1, &chunks);
            for (int i = 0; i < chunks.size(); ++ i) {
                for (auto& elem : chunks[i]) {
                    assert(elem == 1);
                    elem = 2;
                }
            }
            ml::model::DumpAllChunksToKV(info.get_local_id(), kv1, chunks);
            ml::model::LoadAllChunksFromKV(info.get_local_id(), kv1, &chunks);
            for (int i = 0; i < chunks.size(); ++ i) {
                for (auto& elem : chunks[i]) {
                    assert(elem == 2);
                }
            }
        }

        {
            // Test LoadChunks/DumpChunks
            std::vector<size_t> keys{1, 3};
            std::vector<float> chunk1(chunk_size, 4);
            std::vector<float> chunk3(chunk_size, 5);
            std::vector<std::vector<float>*> chunks{&chunk1, &chunk3};
            ml::model::DumpChunks(info.get_local_id(), kv1, keys, chunks);
            std::vector<float> recv1, recv2;
            std::vector<std::vector<float>*> recv{&recv1, &recv2};
            ml::model::LoadChunks(info.get_local_id(), kv1, keys, &recv);
            for (auto elem : recv1) {
                assert(elem == 4);
            }
            for (auto elem : recv2) {
                assert(elem == 5);
            }
        }
    });
    engine.Submit();
    engine.Exit();
    // Stop the kvstore, should stop before mailbox is down
    kvstore::KVStore::Get().Stop();
}
