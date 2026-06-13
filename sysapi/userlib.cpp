#include "emotionos/types.h"
#include "emotionos/syscall.h"
#include <cstdio>
#include <cstring>
#include <cstdarg>
#include <cmath>

extern "C" void e_main(void* arg);

void e_print(const char* str) {
    eipc_msg_t msg;
    msg.type = EIPC_LOG;
    msg.sender = e_sys_task_self();
    msg.code = 0;
    msg.data = (void*)str;
    msg.length = (uint32_t)(strlen(str) + 1);
    msg.value = 0;
    e_sys_ipc_send(0, &msg);
}

void e_printf(const char* fmt, ...) {
    char buf[256];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    e_print(buf);
}
