// ============================================================
// EmotionOS - 微内核公共类型定义
// 所有组件和用户程序都引用此头文件
// ============================================================
#pragma once
#include <cstdint>
#include <cstddef>

#ifdef __cplusplus
extern "C" {
#endif

// ---------- 基础类型 ----------
typedef uint32_t etask_t;       // 任务ID
typedef uint32_t eport_t;       // IPC端口ID
typedef uint32_t eirq_t;        // 中断号
typedef uint32_t ehandle_t;     // 通用句柄
typedef int32_t  estatus_t;     // 返回值

// ---------- 任务优先级 ----------
typedef enum {
    EPRIO_ROOT    = 0,   // 内核级
    EPRIO_HIGH    = 1,   // 组件级 (情感核等)
    EPRIO_NORMAL  = 2,   // 系统服务
    EPRIO_LOW     = 3,   // 用户程序
    EPRIO_IDLE    = 4,   // 空闲
} eprio_t;

// ---------- 任务状态 ----------
typedef enum {
    ETS_READY      = 0,
    ETS_RUNNING    = 1,
    ETS_BLOCKED    = 2,
    ETS_SLEEPING   = 3,
    ETS_TERMINATED = 4,
    ETS_WAITING    = 5,  // 等待IPC消息
} etstate_t;

// ---------- 内存标志 ----------
typedef enum {
    EMEM_READ  = 1,
    EMEM_WRITE = 2,
    EMEM_EXEC  = 4,
    EMEM_RW    = 3,
    EMEM_RWX   = 7,
} emem_flags_t;

// ---------- IPC消息类型 ----------
typedef enum {
    EIPC_USER     = 0,    // 用户自定义
    EIPC_HORMONE  = 1,    // 激素状态广播
    EIPC_EVENT    = 2,    // 外部事件
    EIPC_MEMORY   = 3,    // 记忆查询/写入
    EIPC_COMMAND  = 4,    // 命令
    EIPC_LOG      = 5,    // 日志
} eipc_type_t;

// ---------- IPC消息 ----------
typedef struct {
    eipc_type_t type;
    etask_t     sender;
    uint32_t    code;       // 消息子类型
    uint32_t    length;     // 数据长度 (字节)
    void*       data;       // 数据指针
    union {
        uint64_t value;     // 标量值
        float    fvalue;    // 浮点值
    };
} eipc_msg_t;

// ---------- 系统调用号 ----------
typedef enum {
    // 任务管理
    ESYS_TASK_CREATE   = 0x01,
    ESYS_TASK_EXIT     = 0x02,
    ESYS_TASK_SLEEP    = 0x03,
    ESYS_TASK_YIELD    = 0x04,
    ESYS_TASK_SELF     = 0x05,
    ESYS_TASK_GETINFO  = 0x06,
    
    // IPC
    ESYS_IPC_SEND      = 0x10,
    ESYS_IPC_RECV      = 0x11,
    ESYS_IPC_BIND      = 0x12,
    ESYS_IPC_POLL      = 0x13,
    
    // 内存管理
    ESYS_MEM_ALLOC     = 0x20,
    ESYS_MEM_FREE      = 0x21,
    ESYS_MEM_MAP       = 0x22,
    ESYS_MEM_PIN       = 0x23,
    
    // 中断
    ESYS_IRQ_REGISTER  = 0x30,
    ESYS_IRQ_RAISE     = 0x31,
    ESYS_IRQ_WAIT      = 0x32,
    
    // 情感/记忆内核服务
    ESYS_AFFECT_GET    = 0x40,
    ESYS_AFFECT_SET    = 0x41,
    ESYS_MEMORY_WRITE  = 0x42,
    ESYS_MEMORY_READ   = 0x43,
    ESYS_MEMORY_RECALL = 0x44,
    
    // 系统
    ESYS_SYS_SHUTDOWN  = 0xFF,
} esyscall_t;

// ---------- 任务信息结构 ----------
typedef struct {
    etask_t     id;
    eprio_t     priority;
    etstate_t   state;
    const char* name;
    uint64_t    total_ticks;
    uint64_t    sleep_until;
} etask_info_t;
// ---------- 服务注册条目 ----------
#define ESERVICE_NAME_MAX 48
#define ESERVICE_DESC_MAX 128
#define MAX_SERVICES 32
typedef struct {
  uint32_t    id;
  char        name[ESERVICE_NAME_MAX];
  eport_t     port;
  eprio_t     priority;
  char        description[ESERVICE_DESC_MAX];
  bool        running;
} eservice_entry_t;

// ---------- 系统日志条目 ----------
#define MAX_LOG_ENTRIES 128
#define LOG_LINE_MAX 128
typedef struct {
  uint64_t    tick;
  uint32_t    source_task;
  char        text[LOG_LINE_MAX];
} elog_entry_t;


// ---------- 系统信息 ----------
typedef struct {
    uint64_t    tick;
    uint32_t    task_count;
    uint32_t    active_task_count;
    uint64_t    total_context_switches;
    uint64_t    uptime_ms;
    float       hormone_mean;
    float       hormone_max;
    uint32_t    hormone_max_idx;
} esys_info_t;

#ifdef __cplusplus
}
#endif
