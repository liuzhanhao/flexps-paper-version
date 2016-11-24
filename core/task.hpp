#pragma once

#include "base/serialization.hpp"

namespace husky {

/*
 * The general Task
 */
using base::BinStream;
class Task {
public:
    enum class Type {
        BasicTaskType,
        PSTaskType,
        HogwildTaskType,
        HuskyTaskType
    };

    Task() = default;
    Task(int id, int total_epoch, int num_workers, Type type = Type::BasicTaskType)
        : id_(id), 
        total_epoch_(total_epoch),
        num_workers_(num_workers),
        type_(type)
    {}
    virtual ~Task() {}

    virtual BinStream& serialize(BinStream& bin) const {
        bin << id_ << total_epoch_ << current_epoch_ << num_workers_ << type_;
    }
    virtual BinStream& deserialize(BinStream& bin) {
        bin >> id_ >> total_epoch_ >> current_epoch_ >> num_workers_ >> type_;
    }

    /*
     * TODO What I want to is to make this function act like virtual
     * But I met the priority problem, that a derived class will first match
     * the general template version before the one takes the base class reference
     * So, for now, user need to rewrite the friend functions in derived class
     */
    friend BinStream& operator<<(BinStream& bin, const Task& task) {
        return task.serialize(bin);
    }
    friend BinStream& operator>>(BinStream& bin, Task& task) {
        return task.deserialize(bin);
    }

    // getter
    inline int get_id() const { return id_; }
    inline int get_total_epoch() const { return total_epoch_; }
    inline int get_current_epoch() const { return current_epoch_; }
    inline int get_num_workers() const { return num_workers_; }
    inline Type get_type() const {return type_; }

    inline void inc_epoch() { current_epoch_ += 1; }
protected:
    int id_;

    int total_epoch_;  // total epoch numbers
    int current_epoch_ = 0;

    int num_workers_;  // num of workers needed to run the job

    Type type_;  // task type
};

class PSTask : public Task {
public:
    PSTask() = default;
    PSTask(int id, int total_epoch, int num_workers, int num_servers = 1)
        : num_servers_(num_servers),
          Task(id, total_epoch, num_workers, Type::PSTaskType)
    {}
    // TODO When we add new class member here, we need to override the serialize and
    // deserialize functions!!!
    friend BinStream& operator<<(BinStream& bin, const PSTask& task) {
        return task.serialize(bin);
    }
    friend BinStream& operator>>(BinStream& bin, PSTask& task) {
        return task.deserialize(bin);
    }

    inline int get_num_ps_servers() const {
        return num_servers_;
    }
    inline int get_num_ps_workers() const {
        return num_workers_ - num_servers_;
    }
    inline bool is_worker(int id) const {
        return !is_server(id);
    }
    inline bool is_server(int id) const {
        return id < num_servers_;
    }
private:
    int num_servers_;
};

class HogwildTask : public Task {
public:
    HogwildTask() = default;
    HogwildTask(int id, int total_epoch, int num_workers)
        : Task(id, total_epoch, num_workers, Type::HogwildTaskType) 
    {}
    friend BinStream& operator<<(BinStream& bin, const HogwildTask& task) {
        return task.serialize(bin);
    }
    friend BinStream& operator>>(BinStream& bin, HogwildTask& task) {
        return task.deserialize(bin);
    }
};

class HuskyTask : public Task {
public:
    HuskyTask() = default;
    HuskyTask(int id, int total_epoch, int num_workers)
        : Task(id, total_epoch, num_workers, Type::HuskyTaskType) 
    {}
    friend BinStream& operator<<(BinStream& bin, const HuskyTask& task) {
        return task.serialize(bin);
    }
    friend BinStream& operator>>(BinStream& bin, HuskyTask& task) {
        return task.deserialize(bin);
    }
};

// conversion functions to cast down along the task hierarchy
namespace {
Task get_task(const std::shared_ptr<Task>& task) {
    return *task.get();
}
PSTask get_pstask(const std::shared_ptr<Task>& task) {
    return *dynamic_cast<PSTask*>(task.get());
}
HuskyTask get_huskytask(const std::shared_ptr<Task>& task) {
    return *dynamic_cast<HuskyTask*>(task.get());
}
HogwildTask get_hogwildtask(const std::shared_ptr<Task>& task) {
    return *dynamic_cast<HogwildTask*>(task.get());
}
}  // namespace

}  // namespace husky