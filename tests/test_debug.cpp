#include <cstdio>
#include <cstring>
#include <windows.h>
#include "emotionos/types.h"
#include "emotionos/syscall.h"

int main() {
    setvbuf(stdout, nullptr, _IONBF, 0);
    printf("=== Debug Exit Test ===\n");
    e_kernel_init();

    e_sys_task_create("__runner", EPRIO_ROOT, [](void*) {
        printf("runner: running...\n");
        
        // Create a simple task that just calls exit
        e_sys_task_create("child", EPRIO_NORMAL, [](void*) {
            printf("  child: running\n");
            printf("  child: calling e_sys_task_exit...\n");
            e_sys_task_exit(0);
            printf("  child: AFTER EXIT - SHOULD NOT PRINT\n");  // crash or unreachable
        }, nullptr, 16384);
        
        printf("runner: sleeping 500ms...\n");
        e_sys_task_sleep(500);
        printf("runner: woke up! PASS\n");
        e_sys_shutdown();
        e_sys_task_exit(0);
    }, nullptr, 32768);

    printf("starting scheduler...\n");
    e_kernel_run();
    printf("done.\n");
    return 0;
}
