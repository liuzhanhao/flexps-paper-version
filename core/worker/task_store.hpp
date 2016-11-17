#pragma once

#include <cassert>
#include <functional>
#include <unordered_map>
#include "core/common/task.hpp"
#include "core/common/info.hpp"

namespace husky {

class TaskStore {
public:
    TaskStore() = default;
    void add_task(const Task& task, std::function<void(Info)> func) {
        assert(task_map.find(task.get_task_id()) == task_map.end());
        task_map.insert({task.get_task_id(), {task, func}});
    }
    auto& get_task_map() {
        return task_map;
    }
    auto get_func(int id) {
        return task_map[id].second;
    }

private:
    std::unordered_map<int, std::pair<Task, std::function<void(Info)>>> task_map;
};

}  // namespace husky
