#include "husky/core/channel/push_channel.hpp"
#include "husky/core/objlist.hpp"
#include "worker/engine.hpp"
#include "core/color.hpp"

#include <random>

using namespace husky;

int main(int argc, char** argv) {
    bool rt = init_with_args(argc, argv, {"worker_port", "cluster_manager_host", "cluster_manager_port"});
    if (!rt)
        return 1;

    auto& engine = Engine::Get();

    auto task = TaskFactory::Get().CreateTask<ConfigurableWorkersTask>(5, 4);
    task.set_worker_num({3, 2, 1, 1, 2});
    task.set_worker_num_type({"threads_per_worker", "threads_per_cluster", "local_threads", "threads_traverse_cluster", "threads_on_worker:0"});
    engine.AddTask(task, [](const Info& info) {
      if (info.get_current_epoch() % 4 == 0) {
        husky::LOG_I << RED("Running (3, threads_per_worker) ");
      } else if (info.get_current_epoch() % 4 == 1) {
        husky::LOG_I << RED("Running (2, threads_per_cluster) ");
      } else if (info.get_current_epoch() % 4 == 2) {
        husky::LOG_I << RED("Running (1, local_threads) ");
      } else if (info.get_current_epoch() % 4 == 3) {
        husky::LOG_I << RED("Running (1, threads_traverse_cluster) ");
      } else {
        husky::LOG_I << RED("Running (2, threads_on_worker:2) ");
      }

    });
    engine.Submit();

    engine.Exit();
}