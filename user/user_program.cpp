// EmotionOS User Program Example
// Compiled as a DLL, loaded by program_load()
// Must export e_program_main
#include <cstdio>
#include <cstring>
#include <windows.h>
#include "emotionos/types.h"
#include "emotionos/syscall.h"

extern "C" __declspec(dllexport) void e_program_main(void*) {
    printf("[USER] Hello from user program!\n");
    
    // Get system info
    esys_info_t info = e_sys_get_info();
    printf("[USER] System: tick=%llu tasks=%u uptime=%llums\n", 
           info.tick, info.task_count, info.uptime_ms);
    
    // Affect test
    float h[16], e[8];
    int hc = 16, ec = 8;
    e_sys_affect_get(h, &hc, e, &ec);
    printf("[USER] Hormones: %.3f %.3f %.3f Emotions: %.3f %.3f\n", 
           h[0], h[4], h[8], e[0], e[4]);
    
    // IPC test
    e_sys_ipc_bind(50);
    printf("[USER] Bound to port 50\n");
    
    // Poll for messages
    eipc_msg_t msg;
    int r = e_sys_ipc_poll(&msg);
    printf("[USER] Poll result: %d\n", r);
    
    // Memory
    void* mem = e_sys_mem_alloc(4096, EMEM_RW);
    if (mem) {
        strcpy((char*)mem, "EmotionOS user program was here!");
        printf("[USER] Allocated memory: %s\n", (char*)mem);
        e_sys_mem_free(mem);
    }
    
    printf("[USER] Exiting.\n");
    e_sys_task_exit(0);
}
