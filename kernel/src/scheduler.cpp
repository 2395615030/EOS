#include "scheduler.h"
#include <cstdio>
#include <windows.h>
#include <iostream>
#include <map>
#include <functional>
#include <deque>

// ============================================================
// EmotionOS - 软内核调度器 v3 (简化协程调度)
// 使用Windows Fiber实现协作式多任务
// ============================================================

constexpr size_t DEFAULT_STACK_SIZE = 1024 * 64;

struct TaskEntry {
    uint32_t id;
    std::function<void()> func;
};

static std::map<uint32_t, TaskEntry> g_task_entries;
static bool g_kernel_running = false;
static std::deque<uint32_t> g_ready_queue; // 全局就绪队列
static void* g_main_fiber = nullptr;

Scheduler& Scheduler::instance() {
    static Scheduler inst;
    return inst;
}

bool Scheduler::init() {
    printf("[KERNEL] EmotionOS Kernel init...\n");
    sys_state_.start_time_ms = GetTickCount64();
    g_kernel_running = false;
    g_main_fiber = ConvertThreadToFiber(nullptr);
    if (!g_main_fiber) {
        printf("[KERNEL] WARN: ConvertThreadToFiber\n");
    }
    // 创建空闲和timer任务
    idle_task_id_ = create_task("idle", TaskPriority::IDLE, [this]() { idle_task_entry(); }, 4096);
    printf("[KERNEL] Ready.\n");
    return true;
}

void Scheduler::shutdown() {
    g_kernel_running = false;
    running_ = false;
}

uint32_t Scheduler::create_task(const std::string& name, TaskPriority priority,
                                std::function<void()> entry, size_t stack_size) {
    if (stack_size == 0) stack_size = DEFAULT_STACK_SIZE;
    uint32_t id = next_task_id_++;
    auto tcb = std::make_unique<TCB>(id, name, priority);

    // 创建Fiber
    void* fiber = CreateFiber(stack_size, fiber_entry,
                              reinterpret_cast<void*>(static_cast<uint64_t>(id)));
    if (!fiber) { printf("[KERNEL] CreateFiber failed for %d\n", id); return 0; }
    
    tcb->fiber = fiber;
    g_task_entries[id] = { id, std::move(entry) };
    
    uint32_t real_id = tcb->id;
    tasks_[real_id] = std::move(tcb);
    // 加入就绪队列
    g_ready_queue.push_back(real_id);
    sys_state_.total_tasks_created++;
    printf("[KERNEL] Task %d '%s' prio=%s\n", real_id, name.c_str(), priority_name(priority));
    return real_id;
}

void __stdcall Scheduler::fiber_entry(void* param) {
    uint32_t task_id = static_cast<uint32_t>(reinterpret_cast<uint64_t>(param));
    auto it = g_task_entries.find(task_id);
    if (it != g_task_entries.end()) it->second.func();

    auto* task = Scheduler::instance().get_task(task_id);
    if (task) { task->state = TaskState::TERMINATED; }
    // 切回主调度器
    SwitchToFiber(g_main_fiber);
}

void Scheduler::yield() {
    if (current_task_) {
        current_task_->total_ticks++;
        current_task_->state = TaskState::READY;
        g_ready_queue.push_back(current_task_->id);
    }
    current_task_ = nullptr;
    // 切回主调度器
    SwitchToFiber(g_main_fiber);
}

void Scheduler::sleep(uint64_t ms) {
    if (!current_task_) return;
    current_task_->state = TaskState::SLEEPING;
    current_task_->sleep_until = GetTickCount64() + ms;
    blocked_tasks_[current_task_->id] = current_task_;
    current_task_ = nullptr;
    SwitchToFiber(g_main_fiber);
}

void Scheduler::wakeup(uint32_t id) {
    auto it = tasks_.find(id);
    if (it == tasks_.end()) return;
    if (it->second->state == TaskState::SLEEPING) {
        it->second->state = TaskState::READY;
        g_ready_queue.push_back(id);
        blocked_tasks_.erase(id);
    }
}

TCB* Scheduler::get_task(uint32_t id) {
    auto it = tasks_.find(id);
    return (it != tasks_.end()) ? it->second.get() : nullptr;
}

void Scheduler::idle_task_entry() {
    while (g_kernel_running) { Sleep(10); yield(); }
}

void Scheduler::print_status() {
    printf("\n=== EmotionOS Status ===\n");
    printf("Tick: %llu Tasks: %zu ReadyQ: %zu Blocked: %zu\n",
           sys_state_.tick, tasks_.size(), g_ready_queue.size(), blocked_tasks_.size());
    for (auto& [id, tcb] : tasks_) {
        const char* s = "?";
        switch (tcb->state) {
            case TaskState::READY: s="RDY"; break;
            case TaskState::RUNNING: s="RUN"; break;
            case TaskState::SLEEPING: s="SLP"; break;
            case TaskState::TERMINATED: s="END"; break;
            default: break;
        }
        printf("  [%d] %-10s %s prio=%s t=%llu\n",
               id, tcb->name.c_str(), s, priority_name(tcb->priority), tcb->total_ticks);
    }
    printf("=============================\n");
}

void Scheduler::run() {
    g_kernel_running = true;
    running_ = true;
    sys_state_.context_switches = 0;
    printf("[KERNEL] Scheduler started.\n\n");

    while (g_kernel_running) {
        // 唤醒到时的睡眠任务
        uint64_t now = GetTickCount64();
        for (auto it = blocked_tasks_.begin(); it != blocked_tasks_.end(); ) {
            if (it->second->state == TaskState::SLEEPING && now >= it->second->sleep_until) {
                it->second->state = TaskState::READY;
                g_ready_queue.push_back(it->second->id);
                it = blocked_tasks_.erase(it);
            } else { ++it; }
        }

        // 取下一个就绪任务
        if (!g_ready_queue.empty()) {
            uint32_t next_id = g_ready_queue.front();
            g_ready_queue.pop_front();

            auto it = tasks_.find(next_id);
            if (it == tasks_.end() || it->second->state == TaskState::TERMINATED) continue;
            if (it->second->state != TaskState::READY) continue;

            current_task_ = it->second.get();
            current_task_->state = TaskState::RUNNING;
            sys_state_.context_switches++;
            sys_state_.tick++;

            // 切换到任务Fiber
            SwitchToFiber(current_task_->fiber);
            // 任务yield/sleep后回到这里
            current_task_ = nullptr;
        } else {
            Sleep(1);
        }
    }
}
