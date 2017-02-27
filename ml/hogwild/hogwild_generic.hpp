#pragma once

#include <chrono>

#include "core/info.hpp"
#include "husky/base/exception.hpp"
#include "husky/base/serialization.hpp"
#include "husky/core/zmq_helpers.hpp"

#include "ml/common/mlworker.hpp"
#include "ml/model/load.hpp"
#include "ml/model/dump.hpp"
#include "ml/shared/shared_state.hpp"

#include "kvstore/kvstore.hpp"

#include "core/color.hpp"

namespace ml {
namespace hogwild {

/*
 * For the HogwildGenericWorker, the type ModelType is now fixed to std::vector<float>
 */
class HogwildGenericWorker : public common::GenericMLWorker {
   public:
    HogwildGenericWorker() = delete;
    HogwildGenericWorker(const HogwildGenericWorker&) = delete;
    HogwildGenericWorker& operator=(const HogwildGenericWorker&) = delete;
    HogwildGenericWorker(HogwildGenericWorker&&) = delete;
    HogwildGenericWorker& operator=(HogwildGenericWorker&&) = delete;

    /*
     * constructor to construct a hogwild model
     * \param context zmq_context
     * \param info info in this instance
     */
    HogwildGenericWorker(int model_id, zmq::context_t& context, const husky::Info& info, size_t num_params)
        : shared_state_(info.get_task_id(), info.get_cluster_id(), info.get_num_local_workers(), context),
          info_(info),
          model_id_(model_id) {

        if (info.get_cluster_id() == 0) {
            husky::LOG_I << CLAY("[Hogwild] model_id: "+std::to_string(model_id)
                    +" local_id: "+std::to_string(info.get_local_id()));
        }

        // check valid
        if (!isValid()) {
            throw husky::base::HuskyException("[Hogwild] threads are not in the same machine. Task is:" +
                                              std::to_string(info.get_task_id()));
        }

        if (info_.get_cluster_id() == 0) {
            std::vector<float>* state = new std::vector<float>(num_params);
            // 1. Init shared_state_
            shared_state_.Init(state);
        }
        // 2. Sync shared_state_
        shared_state_.SyncState();
    }

    /*
     * destructor
     * 1. Sync() and 2. leader delete the model
     */
    ~HogwildGenericWorker() {
        shared_state_.Barrier();
        if (info_.get_cluster_id() == 0) {
            delete shared_state_.Get();
        }
    }

    void print_model() const {
        // debug
        for (size_t i = 0; i < shared_state_.Get()->size(); ++i)
            husky::LOG_I << std::to_string((*shared_state_.Get())[i]);
    }

    /*
     * Get parameters from global kvstore
     */
    virtual void Load() override {
        if (info_.get_cluster_id() == 0) {
            model::LoadAllIntegral(info_.get_local_id(), model_id_, shared_state_.Get()->size(), shared_state_.Get());
        }
        // Other threads should wait
        shared_state_.Barrier();
    }
    /*
     * Put the parameters to global kvstore
     */
    virtual void Dump() override {
        shared_state_.Barrier();
        if (info_.get_cluster_id() == 0) {
            model::DumpAllIntegral(info_.get_local_id(), model_id_, shared_state_.Get()->size(), *shared_state_.Get());
        }
        shared_state_.Barrier();
    }

    /*
     * Put/Get Push/Pull APIs
     */
    virtual void Put(size_t key, float val) {
        assert(key < shared_state_.Get()->size());
        (*shared_state_.Get())[key] += val;
    }
    virtual float Get(size_t key) {
        assert(key < shared_state_.Get()->size());
        return (*shared_state_.Get())[key];
    }
    virtual void Push(const std::vector<husky::constants::Key>& keys, const std::vector<float>& vals) override {
        assert(keys.size() == vals.size());
        for (size_t i = 0; i < keys.size(); i++) {
            assert(keys[i] < shared_state_.Get()->size());
            (*shared_state_.Get())[keys[i]] += vals[i];
        }
    }
    virtual void Pull(const std::vector<husky::constants::Key>& keys, std::vector<float>* vals) override {
        vals->resize(keys.size());
        for (size_t i = 0; i < keys.size(); i++) {
            assert(keys[i] < shared_state_.Get()->size());
            (*vals)[i] = (*shared_state_.Get())[keys[i]];
        }
    }

    /*
     * Get the model
     */
    std::vector<float>* get() { return shared_state_.Get(); }

    /*
     * Serve as a barrier
     */
    virtual void Sync() override {
        shared_state_.Barrier();
    }

    // For v2
    virtual void Prepare_v2(const std::vector<husky::constants::Key>& keys) override {
        keys_ = const_cast<std::vector<husky::constants::Key>*>(&keys);
    }
    virtual float Get_v2(size_t idx) override { return (*shared_state_.Get())[(*keys_)[idx]]; }
    virtual void Update_v2(size_t idx, float val) override { (*shared_state_.Get())[(*keys_)[idx]] += val; }
    virtual void Update_v2(const std::vector<float>& vals) override {
        assert(vals.size() == keys_->size());
        for (size_t i = 0; i < keys_->size(); ++i) {
            assert((*keys_)[i] < shared_state_.Get()->size());
            (*shared_state_.Get())[(*keys_)[i]] += vals[i];
        }
    }

   private:
    /*
     * check whether all the threads are in the same machine
     */
    bool isValid() {
        // husky::base::log_msg("locals: " + std::to_string(info_.get_num_local_workers()) + " globals:" +
        //                      std::to_string(info_.get_num_workers()));
        return info_.get_num_local_workers() == info_.get_num_workers();
    }

    const husky::Info& info_;
    SharedState<std::vector<float>> shared_state_;
    int model_id_;

    // For v2
    // Pointer to keys
    std::vector<husky::constants::Key>* keys_;
};

}  // namespace hogwild
}  // namespace ml
