// ============================================================
// EmotionOS - 系统调用接口
// 内核暴露给所有组件的底层API
// ============================================================
#pragma once
#include "types.h"
#include <cstring>
#include <cstdio>

#ifdef __cplusplus
extern "C" {
#endif

// ---- 任务管理 ----
etask_t  e_sys_task_create(const char* name, eprio_t priority,
                           void (*entry)(void*), void* arg,
                           size_t stack_size);
void     e_sys_task_exit(int code);
void     e_sys_task_sleep(uint32_t ms);
void     e_sys_task_yield(void);
etask_t  e_sys_task_self(void);
estatus_t e_sys_task_getinfo(etask_t id, etask_info_t* info);
estatus_t e_sys_task_kill(etask_t id);
estatus_t e_sys_task_set_priority(etask_t id, eprio_t new_prio);
estatus_t e_sys_task_list(etask_t* out_ids, uint32_t* out_count, uint32_t max);
estatus_t e_sys_task_suspend(etask_t id);
estatus_t e_sys_task_resume(etask_t id);



// ---- IPC ----
estatus_t e_sys_ipc_bind(eport_t port);
estatus_t e_sys_ipc_send(etask_t target, const eipc_msg_t* msg);
estatus_t e_sys_ipc_recv(eipc_msg_t* msg, uint32_t timeout_ms);
estatus_t e_sys_ipc_poll(eipc_msg_t* msg);

// ---- 内存 ----
void*    e_sys_mem_alloc(size_t size, emem_flags_t flags);
estatus_t e_sys_mem_free(void* addr);
estatus_t e_sys_mem_pin(void* addr, size_t size);
estatus_t e_sys_mem_unpin(void* addr, size_t size);

// ---- 软中断 ----
eirq_t   e_sys_irq_register(void (*handler)(void*), void* arg);
estatus_t e_sys_irq_raise(etask_t target, eirq_t irq);
estatus_t e_sys_irq_wait(eirq_t irq, uint32_t timeout_ms);

// ---- 情感/记忆内核服务 ----
estatus_t e_sys_affect_get(float* hormones, int* h_count,
                           float* emotions, int* e_count);
estatus_t e_sys_affect_inject(const float* stimulus, int count);
estatus_t e_sys_memory_write(const float* data, int count, float emotion_tag);
estatus_t e_sys_memory_recall(float* output, int max_count, float temperature);
// ---- 服务注册表 ----
estatus_t e_sys_service_register(const char* name, eport_t port, eprio_t priority, const char* description);
estatus_t e_sys_service_lookup(const char* name, eservice_entry_t* out);
estatus_t e_sys_service_list(eservice_entry_t* out, uint32_t* count, uint32_t max);
estatus_t e_sys_service_unregister(const char* name);

// ---- 系统日志 ----
void     e_sys_log(const char* text);
estatus_t e_sys_log_read(elog_entry_t* out, uint32_t* count, uint32_t max);
void     e_sys_logf(const char* fmt, ...);


// ---- 系统 ----
void     e_sys_shutdown(void);
esys_info_t e_sys_get_info(void);

// ---- 内核启动函数 ----
void e_kernel_init(void);
void e_kernel_run(void);
estatus_t e_sys_program_load(const char* dll_path, const char* task_name, eprio_t priority);

#ifdef __cplusplus
}
#endif
