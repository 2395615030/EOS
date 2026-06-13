#pragma once
#include <cstdint>
#include <cstddef>
#include <functional>
#include <memory>
#include <vector>
#include <array>
#include <chrono>
#include <string>

// ============================================================
// EmotionOS 软内核 - 核心类型定义
// ============================================================

// ---------- 任务优先级 (对应三重权限模型) ----------
enum class TaskPriority : uint8_t {
    ROOT     = 0,  // 根权限 (核级, 不可抢占)
    LOGIC    = 1,  // 逻辑权限 (系统服务)
    USER     = 2,  // 用户权限 (应用级)
    IDLE     = 3,  // 空闲任务
    
    _COUNT
};

inline const char* priority_name(TaskPriority p) {
    switch (p) {
        case TaskPriority::ROOT:  return "ROOT";
        case TaskPriority::LOGIC: return "LOGIC";
        case TaskPriority::USER:  return "USER";
        case TaskPriority::IDLE:  return "IDLE";
        default: return "UNKN";
    }
}

// ---------- 任务状态 ----------
enum class TaskState : uint8_t {
    READY,       // 就绪
    RUNNING,     // 运行中
    BLOCKED,     // 阻塞 (等待事件/资源)
    SLEEPING,    // 休眠 (定时唤醒)
    TERMINATED   // 已终止
};

// ---------- 软中断类型 ----------
enum class SoftIRQ : uint8_t {
    NONE = 0,
    HORMONE_THRESHOLD,   // 激素超阈值
    TIMER_TICK,          // 时钟节拍
    MEMORY_READY,        // 记忆加载完成
    EVENT_INPUT,         // 外部事件输入
    IPC_MESSAGE,         // 进程间消息
    SCHEDULER_YIELD,     // 主动让出
    _COUNT
};

inline const char* irq_name(SoftIRQ irq) {
    switch (irq) {
        case SoftIRQ::HORMONE_THRESHOLD: return "HORMONE";
        case SoftIRQ::TIMER_TICK:        return "TIMER";
        case SoftIRQ::MEMORY_READY:      return "MEMRDY";
        case SoftIRQ::EVENT_INPUT:       return "EVENT";
        case SoftIRQ::IPC_MESSAGE:       return "IPC";
        case SoftIRQ::SCHEDULER_YIELD:   return "YIELD";
        default: return "NONE";
    }
}

// ---------- 中断帧 (上下文保存) ----------
struct InterruptFrame {
    uint64_t rsp;        // 栈指针
    uint64_t rflags;     // 标志寄存器
    uint64_t rip;        // 指令指针
    // 可扩展
};

// ---------- 任务控制块 (TCB) ----------
struct TCB {
    uint32_t id;                       // 任务ID
    std::string name;                  // 任务名称
    TaskPriority priority;             // 优先级
    TaskState state;                   // 状态
    
    // Fiber句柄 (Windows Fiber)
    void* fiber = nullptr;
    void* original_fiber = nullptr;    // 主Fiber
    
    // 时间片统计
    uint64_t total_ticks = 0;          // 总运行节拍数
    uint64_t last_run_tick = 0;        // 上次运行时间戳
    uint64_t sleep_until = 0;          // 唤醒时间 (SLEEPING状态)
    
    // 软中断挂起
    SoftIRQ pending_irq = SoftIRQ::NONE;
    
    // 上下文数据指针
    void* context = nullptr;
    
    // 构造函数
    TCB(uint32_t id, std::string name, TaskPriority prio)
        : id(id), name(std::move(name)), priority(prio), state(TaskState::READY) {}
};

// ---------- 内存区域描述 ----------
enum class MemType : uint8_t {
    L0_COLD,     // NVMe冷仓
    L1_HOT,      // DDR热池
    L2_COMPUTE,  // VRAM计算片
    SHARED       // 共享内存
};

struct MemRegion {
    void*       virt_addr = nullptr;
    uint64_t    size = 0;
    MemType     type = MemType::L1_HOT;
    bool        pinned = false;   // 锁定在物理内存
    uint32_t    owner_task = 0;   // 所属任务
};

// ---------- 系统状态 ----------
struct SystemState {
    uint64_t tick = 0;             // 全局节拍
    uint64_t start_time_ms;        // 启动时间
    uint64_t total_tasks_created = 0;
    uint64_t context_switches = 0;
    
    // 情感内核状态快照 (由情感核服务更新)
    float hormone_mean = 0.5f;
    float emotion_dominant = 0.0f;
    uint32_t hormone_max_idx = 0;
    
    // 系统负载
    float cpu_usage = 0.0f;
    uint32_t ready_queue_size = 0;
};
