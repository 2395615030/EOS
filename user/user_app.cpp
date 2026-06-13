// EmotionOS User Program (standalone + embedded kernel)
// Compile as: g++ -o user_app.exe user_app.cpp ../kernel/src/kernel.cpp -I../kernel/include -lpthread
// Shows how to use the EmotionOS API to build applications
#include <cstdio>
#include <cstring>
#include <windows.h>
#include "emotionos/types.h"
#include "emotionos/syscall.h"

static void user_component(void*) {
    printf("[USER] Application component started!\n");
    printf("[USER] Task ID: %u\n", e_sys_task_self());
    
    // === System Info ===
    esys_info_t sys = e_sys_get_info();
    printf("[USER] System uptime: %llu ms, %u tasks active\n", 
           sys.uptime_ms, sys.active_task_count);
    
    // === Memory ===
    printf("[USER] === Memory Test ===\n");
    void* buf = e_sys_mem_alloc(8192, EMEM_RW);
    if (buf) {
        snprintf((char*)buf, 8192, "EmotionOS user application data buffer");
        printf("[USER] Allocated: %s\n", (char*)buf);
        e_sys_mem_free(buf);
        printf("[USER] Freed successfully\n");
    }
    
    // === IPC ===
    printf("[USER] === IPC Test ===\n");
    e_sys_ipc_bind(80);
    printf("[USER] Bound to port 80\n");
    
    // Create a worker task
    volatile int worker_result = 0;
    auto worker = [](void* arg) {
        int* result = (int*)arg;
        e_sys_ipc_bind(81);
        printf("[WORKER] Started, waiting for message...\n");
        
        eipc_msg_t msg;
        if (e_sys_ipc_recv(&msg, 3000) == 0) {
            printf("[WORKER] Received: code=%u value=%llu\n", msg.code, msg.value);
            *result = (int)msg.value;
        } else {
            printf("[WORKER] Timeout\n");
            *result = -1;
        }
        e_sys_task_exit(0);
    };
    
    etask_t worker_id = e_sys_task_create("worker", EPRIO_NORMAL, worker, (void*)&worker_result, 16384);
    if (worker_id) {
        e_sys_task_sleep(100);
        
        // Send directed message
        eipc_msg_t msg;
        memset(&msg, 0, sizeof(msg));
        msg.type = EIPC_USER;
        msg.code = 42;
        msg.value = 999;
        int sr = e_sys_ipc_send(worker_id, &msg);
        printf("[USER] Sent to worker (id=%u): result=%d\n", worker_id, sr);
        
        e_sys_task_sleep(500);
        printf("[USER] Worker result: %d (expected 999)\n", worker_result);
    }
    
    // === Affect System ===
    printf("[USER] === Affect System Test ===\n");
    float h[16], e[8];
    int hc = 16, ec = 8;
    if (e_sys_affect_get(h, &hc, e, &ec) == 0) {
        printf("[USER] Hormones: ");
        for (int i = 0; i < 4; i++) printf("%.3f ", h[i]);
        printf("\n[USER] Emotions: ");
        for (int i = 0; i < 4; i++) printf("%+.3f ", e[i]);
        printf("\n");
    }
    
    // Inject a stimulus
    float stim[128] = {0};
    for (int i = 48; i < 64; i++) stim[i] = 0.85f;
    e_sys_affect_inject(stim, 128);
    printf("[USER] Injected reward stimulus\n");
    
    // === Interrupts ===
    printf("[USER] === Interrupt Test ===\n");
    volatile int irq_fired = 0;
    eirq_t irq = e_sys_irq_register([](void* p) { 
        (*(volatile int*)p)++; 
        printf("[IRQ] Handler fired!\n");
    }, (void*)&irq_fired);
    printf("[USER] Registered IRQ %u\n", irq);
    
    etask_t self = e_sys_task_self();
    e_sys_irq_raise(self, irq);
    printf("[USER] Raised IRQ to self\n");
    
    // === Task Management ===
    printf("[USER] === Task Management Test ===\n");
    etask_info_t info;
    if (e_sys_task_getinfo(e_sys_task_self(), &info) == 0) {
        printf("[USER] Self: id=%u name=%s priority=%d\n", 
               info.id, info.name ? info.name : "?", info.priority);
    }
    
    // Create multiple short-lived tasks
    printf("[USER] Creating 5 short-lived tasks...\n");
    volatile int task_count = 0;
    auto short_task = [](void* arg) {
        (*(volatile int*)arg)++;
        e_sys_task_exit(0);
    };
    for (int i = 0; i < 5; i++) {
        char name[16]; snprintf(name, 16, "st%d", i);
        e_sys_task_create(name, EPRIO_NORMAL, short_task, (void*)&task_count, 4096);
    }
    e_sys_task_sleep(500);
    printf("[USER] %d/5 short tasks completed\n", task_count);
    
    printf("[USER] Application test complete!\n"); printf("[USER] Shutting down...\n"); e_sys_shutdown();
    e_sys_task_exit(0);
}

int main() {
    setvbuf(stdout, nullptr, _IONBF, 0);
    printf("========================================\n");
    printf(" EmotionOS User Application Demo\n");
    printf("========================================\n\n");
    
    e_kernel_init();
    
    e_sys_task_create("idle", EPRIO_IDLE, [](void*){ while(1){ Sleep(10); e_sys_task_yield(); } }, nullptr, 4096);
    e_sys_task_create("user_app", EPRIO_NORMAL, user_component, nullptr, 32768);
    
    printf("[BOOT] Starting user application...\n");
    e_kernel_run();
    
    printf("[BOOT] Application halted.\n");
    return 0;
}

