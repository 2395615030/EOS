#include <cstdio>
#include <cstring>
#include <windows.h>
#include "emotionos/types.h"
#include "emotionos/syscall.h"

static int g_val = 0;
static void exit_fn(void*) { g_val = 42; printf("exiter: g_val=%d, calling e_sys_task_exit\n", g_val); e_sys_task_exit(0); }

int main() {
    setvbuf(stdout, nullptr, _IONBF, 0);
    printf("=== Minimal Exit Test ===\n");
    e_kernel_init();
    
    e_sys_task_create("__watchdog", EPRIO_ROOT, [](void*) {
        e_sys_task_sleep(5000);
        printf("[WATCHDOG] timeout\n");
        e_sys_shutdown();
        e_sys_task_exit(0);
    }, nullptr, 4096);

    e_sys_task_create("__runner", EPRIO_ROOT, [](void*) {
        printf("runner: creating exiter task\n");
        e_sys_task_create("exiter", EPRIO_NORMAL, exit_fn, nullptr, 16384);
        printf("runner: sleeping 500ms...\n");
        e_sys_task_sleep(500);
        printf("runner: woke up, g_val=%d\n", g_val);
        if (g_val == 42) printf("PASS\n"); else printf("FAIL\n");
        e_sys_task_sleep(100);
        e_sys_shutdown();
        e_sys_task_exit(0);
    }, nullptr, 32768);

    printf("starting scheduler...\n");
    e_kernel_run();
    printf("done.\n");
    return 0;
}
