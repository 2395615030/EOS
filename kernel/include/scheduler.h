#pragma once
#include "emotionos.h"
#include <queue>
#include <map>
#include <functional>
#include <vector>

// ============================================================
// EmotionOS - 软内核调度器
// 基于Windows Fiber的多优先级抢占式调度
// ============================================================

class Scheduler {
public:
    // 单例
    static Scheduler& instance();
    
    // ---- 初始化 ----
    bool init();
    void shutdown();
    bool is_running() const { return running_; }
    
    // ---- 任务管理 ----
    uint32_t create_task(
        const std::string& name,
        TaskPriority priority,
        std::function<void()> entry,
        size_t stack_size = 0
    );
    
    bool terminate_task(uint32_t id);
    TCB* get_task(uint32_t id);
    
    // ---- 调度 ----
    void yield();                     // 主动让出CPU
    void sleep(uint64_t ms);          // 睡眠指定毫秒数
    void wakeup(uint32_t id);         // 唤醒指定任务
    
    // ---- 软中断 ----
    void raise_irq(SoftIRQ irq, uint32_t target_task = 0);
    SoftIRQ poll_irq(uint32_t task_id);
    
    // ---- 内存管理 ----
    void* alloc_region(uint64_t size, MemType type);
    bool free_region(void* addr);
    bool pin_region(void* addr);      // 锁定到物理内存
    bool unpin_region(void* addr);
    
    // ---- 系统信息 ----
    uint64_t tick() const { return sys_state_.tick; }
    const SystemState& system_state() const { return sys_state_; }
    void print_status();
    
    // ---- 运行 ----
    void run();  // 启动调度器, 永不返回
    
private:
    Scheduler() = default;
    ~Scheduler() = default;
    Scheduler(const Scheduler&) = delete;
    Scheduler& operator=(const Scheduler&) = delete;
    
    // 内部调度逻辑
    TCB* pick_next_task();
    void switch_to(TCB* task);
    void on_tick();                   // 时钟节拍处理
    
    // 内部任务
    void idle_task_entry();           // 空闲任务
    void timer_task_entry();          // 时钟任务
    
    // Windows Fiber相关
    static void __stdcall fiber_entry(void* param);
    
    // 状态
    bool running_ = false;
    SystemState sys_state_;
    
    // 任务表
    std::map<uint32_t, std::unique_ptr<TCB>> tasks_;
    uint32_t next_task_id_ = 1;
    
    // 就绪队列: 每优先级一个队列
    std::array<std::queue<uint32_t>, static_cast<size_t>(TaskPriority::_COUNT)> ready_queues_;
    
    // 阻塞/睡眠队列
    std::map<uint32_t, TCB*> blocked_tasks_;
    
    // 当前任务
    TCB* current_task_ = nullptr;
    
    // 空闲任务ID
    uint32_t idle_task_id_ = 0;
    uint32_t timer_task_id_ = 0;
    
    // 系统内存区域
    std::vector<MemRegion> mem_regions_;
    
    // 软中断队列
    std::multimap<uint32_t, SoftIRQ> irq_queue_;
};
