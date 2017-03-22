#include "gtest/gtest.h"

#include "ml/mlworker/ps.hpp"

#include "boost/thread.hpp"
#include "core/instance.hpp"
#include "core/info.hpp"
#include "core/utility.hpp"

namespace husky {
namespace {

/*
 * Test different features:
 * PSSharedChunkWorker: process_cache, chunk-based
 * PSSharedWorker:      process_cache
 * SSPWorkerChunk:      chunk-based
 * SSPWorker
 */
class TestPS: public testing::Test {
   public:
    TestPS() {}
    ~TestPS() {}

   protected:
    void SetUp() {
        zmq_context = new zmq::context_t;
        worker_info = new WorkerInfo;
        // 1. Create WorkerInfo
        worker_info->add_worker(0,0,0);
        worker_info->add_worker(0,1,1);
        worker_info->set_process_id(0);

        // 2. Create Mailbox
        el = new MailboxEventLoop(zmq_context);
        el->set_process_id(0);
        recver = new CentralRecver(zmq_context, "inproc://test");
        // 3. Start KVStore
        kvstore::KVStore::Get().Start(*worker_info, el, zmq_context);
    }

    void TearDown() {
        kvstore::KVStore::Get().Stop();
        delete worker_info;
        delete el;
        delete recver;
        delete zmq_context;
    }

   public:
    int num_params = 100;
    WorkerInfo* worker_info;
    zmq::context_t* zmq_context;
    MailboxEventLoop* el;
    CentralRecver * recver;
};

TEST_F(TestPS, Construct) {
    std::map<std::string, std::string> hint = {
        {husky::constants::kParamType, husky::constants::kChunkType},
        {husky::constants::kConsistency, husky::constants::kSSP},
        {husky::constants::kType, husky::constants::kPS},
        {husky::constants::kNumWorkers, "2"},
        {husky::constants::kStaleness, "2"}
    };
    int kv1 = kvstore::KVStore::Get().CreateKVStore<float>(hint, num_params, 2);
    // Create a task
    husky::MLTask task(0);
    task.set_total_epoch(1);
    task.set_dimensions(num_params);
    task.set_kvstore(kv1);
    task.set_hint(hint);
    // Create an Instance
    husky::Instance instance;
    instance.add_thread(0, 0, 0);  // pid, tid, cid
    instance.set_task(task);
    // Create an Info
    husky::Info info = husky::utility::instance_to_info(instance, *worker_info, {0, 0}, true);
    // Create PSSharedChunkWorker
    ml::mlworker::PSSharedChunkWorker<float> worker1(info, *zmq_context);
    /*
    // Create PSSharedWorker
    ml::mlworker::PSSharedWorker<float> worker2(info, *zmq_context);
    */
    // Create SSPWorkerChunk
    ml::mlworker::SSPWorkerChunk<float> worker3(info);
    // Create SSPWorker
    ml::mlworker::SSPWorker<float> worker4(info);
}

void testPushPull(ml::mlworker::GenericMLWorker<float>* worker) {
    // PushPull
    std::vector<husky::constants::Key> keys = {0,10,20,30,40,50,60,70,80,90};
    std::vector<float> old_vals;
    std::vector<float> vals(keys.size(), 0.1);
    worker->Pull(keys, &old_vals);
    worker->Push(keys, vals);
}

void testV2(ml::mlworker::GenericMLWorker<float>* worker) {
    // v2 APIs
    std::vector<husky::constants::Key> keys = {0,10,20,30,40,50,60,70,80,90};
    worker->Prepare_v2(keys);
    std::vector<float> old_vals;
    std::vector<float> vals(keys.size(), 0.1);
    for (int i = 0; i < keys.size(); ++ i)
        old_vals.push_back(worker->Get_v2(i));
    for (int i = 0; i < keys.size(); ++ i)
        worker->Update_v2(i, vals[i]);
    worker->Clock_v2();
}

void test_multiple_threads(TestPS* obj, int type) {
    std::map<std::string, std::string> hint = {
        {husky::constants::kParamType, husky::constants::kChunkType},
        {husky::constants::kConsistency, husky::constants::kSSP},
        {husky::constants::kType, husky::constants::kPS},
        {husky::constants::kNumWorkers, "2"},
        {husky::constants::kStaleness, "2"}
    };
    int kv1 = kvstore::KVStore::Get().CreateKVStore<float>(hint, 100, 2);
    // Create a task
    husky::MLTask task(0);
    task.set_total_epoch(2);
    task.set_dimensions(100);
    task.set_kvstore(kv1);
    task.set_hint(hint);
    // Create an Instance
    husky::Instance instance;
    instance.add_thread(0, 0, 0);  // pid, tid, cid
    instance.add_thread(0, 1, 1);  // pid, tid, cid
    instance.set_task(task);

    int iters = 10;

    boost::thread t1([&instance, &obj, &iters, &type](){
        husky::Info info = husky::utility::instance_to_info(instance, *obj->worker_info, {0, 0}, true);
        if (type == 3) {
            ml::mlworker::PSSharedChunkWorker<float> worker(info, *obj->zmq_context);
            for (int i = 0; i < iters; ++i) {
                testPushPull(&worker);
                testV2(&worker);
            }
        } else if (type == 2) {
            ml::mlworker::PSSharedWorker<float> worker(info, *obj->zmq_context);
            for (int i = 0; i < iters; ++i) {
                testPushPull(&worker);
                testV2(&worker);
            }
        } else if (type == 1) {
            ml::mlworker::SSPWorkerChunk<float> worker(info);
            for (int i = 0; i < iters; ++i) {
                testPushPull(&worker);
                testV2(&worker);
            }
        } else if (type == 0) {
            ml::mlworker::SSPWorker<float> worker(info);
            for (int i = 0; i < iters; ++i) {
                testPushPull(&worker);
                testV2(&worker);
            }
        }
    });
    boost::thread t2([&instance, &obj, &iters, &type](){
        husky::Info info = husky::utility::instance_to_info(instance, *obj->worker_info, {1, 1}, false);
        if (type == 3) {
            ml::mlworker::PSSharedChunkWorker<float> worker(info, *obj->zmq_context);
            for (int i = 0; i < iters; ++i) {
                if (i % 3 == 0) std::this_thread::sleep_for(std::chrono::milliseconds(100));
                testPushPull(&worker);
                testV2(&worker);
            }
        } else if (type == 2) {
            ml::mlworker::PSSharedWorker<float> worker(info, *obj->zmq_context);
            for (int i = 0; i < iters; ++i) {
                if (i % 3 == 0) std::this_thread::sleep_for(std::chrono::milliseconds(100));
                testPushPull(&worker);
                testV2(&worker);
            }
        } else if (type == 1) {
            ml::mlworker::SSPWorkerChunk<float> worker(info);
            for (int i = 0; i < iters; ++i) {
                if (i % 3 == 0) std::this_thread::sleep_for(std::chrono::milliseconds(100));
                testPushPull(&worker);
                testV2(&worker);
            }
        } else if (type == 0) {
            ml::mlworker::SSPWorker<float> worker(info);
            for (int i = 0; i < iters; ++i) {
                if (i % 3 == 0) std::this_thread::sleep_for(std::chrono::milliseconds(100));
                testPushPull(&worker);
                testV2(&worker);
            }
        }
    });
    t1.join();
    t2.join();
}

TEST_F(TestPS, PSSharedChunkWorker) {
    test_multiple_threads(static_cast<TestPS*>(this), 3);
}

TEST_F(TestPS, PSSharedWorker) {
    test_multiple_threads(static_cast<TestPS*>(this), 2);
}

TEST_F(TestPS, SSPWorkerChunk) {
    test_multiple_threads(static_cast<TestPS*>(this), 1);
}

TEST_F(TestPS, SSPWorker) {
    test_multiple_threads(static_cast<TestPS*>(this), 0);
}

}  // namespace
}  // namespace husky
