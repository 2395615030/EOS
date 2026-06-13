#include <cstdio>
#include <cstring>
#include <windows.h>
#include "emotionos/types.h"
#include "emotionos/syscall.h"

int main() {
    setvbuf(stdout, nullptr, _IONBF, 0);
    printf("=== Tick Test ===\n");
    e_kernel_init();

    e_sys_task_create("__watchdog", EPRIO_ROOT, [](void*) {
        e_sys_task_sleep(8000);
        printf("[WATCHDOG] timeout\n");
        e_sys_shutdown();
        e_sys_task_exit(0);
    }, nullptr, 4096);

    e_sys_task_create("__runner", EPRIO_ROOT, [](void*) {
        printf("runner: creating sleeper...\n");
        e_sys_task_create("sleeper", EPRIO_NORMAL, [](void*) {
            printf("  sleeper: running, sleeping 200ms...\n");
            e_sys_task_sleep(200);
            printf("  sleeper: woke up!\n");
            e_sys_task_exit(0);
        }, nullptr, 16384);
        printf("runner: now sleeping 500ms...\n");
        e_sys_task_sleep(500);
        printf("runner: woke up! checking sleeper...\n");
        esys_info_t info = e_sys_get_info();
        printf("runner: tick=%llu tasks=%u active=%u\n", info.tick, info.task_count, info.active_task_count);
        e_sys_task_sleep(100);
        e_sys_shutdown();
        e_sys_task_exit(0);
    }, nullptr, 32768);

    printf("starting scheduler...\n");
    e_kernel_run();
    printf("done.\n");
    return 0;
}
