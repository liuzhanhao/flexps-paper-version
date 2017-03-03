#pragma once

#include <functional>
#include <vector>

#include "core/constants.hpp"
#include "husky/base/exception.hpp"

namespace ml {
namespace mlworker {

/*
 * It serves as an interface for all the parameter-update model in ML
 *
 * TODO: Now we assume the parameters are float
 */
class GenericMLWorker {
   public:
    using Callback = std::function<void()>;

    virtual ~GenericMLWorker() {}
    /*
     * Probably we need an initialize function ?
     */
    virtual void Load() { throw husky::base::HuskyException("Load Not implemented"); }
    virtual void Dump() { throw husky::base::HuskyException("Dump Not implemented"); }
    virtual void Sync() { throw husky::base::HuskyException("Sync Not implemented"); }
    /*
     * Push/Pull APIs are very suitable for PS, but may not be suitable for
     * Hogwild! and Single
     */
    virtual void Push(const std::vector<husky::constants::Key>& keys, const std::vector<float>& vals) {
        throw husky::base::HuskyException("Push Not implemented");
    }
    virtual void Pull(const std::vector<husky::constants::Key>& keys, std::vector<float>* vals) {
        throw husky::base::HuskyException("Pull Not implemented");
    }

    /*
     * Put/Get APIs
     */
    virtual void Put(size_t key, float val) { throw husky::base::HuskyException("Put Not implemented"); }
    virtual float Get(size_t key) { throw husky::base::HuskyException("Get Not implemented"); }

    /*
     * Version 2 APIs, under experiment
     *
     * These set of APIs is to avoid making a copy for Single/Hogwild!
     */
    // Caution: keys should be remained valid during update
    virtual void Prepare_v2(const std::vector<husky::constants::Key>& keys) {
        throw husky::base::HuskyException("v2 Not implemented");
    }
    virtual float Get_v2(size_t idx) { throw husky::base::HuskyException("v2 Not implemented"); }
    virtual void Update_v2(size_t idx, float val) { throw husky::base::HuskyException("v2 Not implemented"); }
    virtual void Update_v2(const std::vector<float>& vals) { throw husky::base::HuskyException("v2 Not implemented"); }
    virtual void Clock_v2(){};  // only for PS
};

}  // namespace mlworker
}  // namespace ml