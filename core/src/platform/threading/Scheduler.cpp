#include "aegis/platform/Scheduler.h"
#include <pthread.h>
#include <sched.h>
#include <spdlog/spdlog.h>
#include <cstring> // strerror

namespace aegis::platform {

    bool Scheduler::set_realtime_priority(int priority) {
        struct sched_param param;
        param.sched_priority = priority;

        // SCHED_FIFO: First In, First Out Realtime policy
        int result = pthread_setschedparam(pthread_self(), SCHED_FIFO, &param);

        if (result != 0) {
            // Usually fails due to permission denied (EPERM)
            spdlog::warn("[Scheduler] Failed to set RT Priority {}: {}", priority, std::strerror(result));
            return false;
        }

        spdlog::info("[Scheduler] Thread elevated to SCHED_FIFO Priority {}", priority);
        return true;
    }

    void Scheduler::set_thread_name(const std::string& name) {
        // Limit 15 chars for Linux
        std::string short_name = name.substr(0, 15);
        pthread_setname_np(pthread_self(), short_name.c_str());
    }

    bool Scheduler::set_cpu_affinity(int core_id) {
        cpu_set_t cpuset;
        CPU_ZERO(&cpuset);
        CPU_SET(core_id, &cpuset);

        int result = pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset);

        if (result != 0) {
            spdlog::warn("[Scheduler] Failed to pin to Core {}: {}", core_id, std::strerror(result));
            return false;
        }
        return true;
    }
}
