// ============================================================
// EmotionOS - 情感核组件
// 内核级组件，通过IPC提供服务
// ============================================================
#include "emotionos/types.h"
#include "emotionos/syscall.h"
#include <cstdio>
#include <cmath>
#include <cstring>
#include <cmath>
#include <random>

static float H[16], E[8];
static float A[16][16], C[16][128];
static std::mt19937 rng(42);
static uint64_t step = 0;

static void init() {
    std::uniform_real_distribution<float> d(-0.3f, 0.3f);
    for (int i = 0; i < 16; i++) {
        H[i] = 0.3f + d(rng) * 0.1f;
        for (int j = 0; j < 16; j++) A[i][j] = (i==j) ? 0.7f : d(rng) * 0.1f;
        for (int j = 0; j < 128; j++) C[i][j] = d(rng) * 0.15f;
    }
    for (int i = 0; i < 8; i++) E[i] = 0;
}

static void step_fn(const float* stim) {
    std::uniform_real_distribution<float> n(-0.05f, 0.05f);
    for (int i = 0; i < 16; i++) {
        float ah=0, cs=0;
        for (int j = 0; j < 16; j++) ah += A[i][j] * H[j];
        for (int j = 0; j < 128; j++) cs += C[i][j] * stim[j];
        H[i] = 1.0f / (1.0f + expf(-(ah + cs + n(rng)))) * 0.97f;
    }
    float hm = 0; for (int i = 0; i < 16; i++) hm += H[i]; hm /= 16;
    float ph = step * 0.1f;
    for (int i = 0; i < 8; i++)
        E[i] = tanhf(hm * 0.5f + n(rng) + 0.3f * sinf(ph + i * 0.5f));
    step++;
}

void affect_entry(void*) {
    init();
    printf("[COMPONENT] Affect Core started.\n");
    e_sys_ipc_bind(1);

    float stim[128] = {};
    std::mt19937 lrng(12345);
    uint64_t last_rp = 0;

    while (1) {
        eipc_msg_t msg;
        if (e_sys_ipc_recv(&msg, 100) == 0 && msg.type == EIPC_EVENT && msg.data)
            memcpy(stim, msg.data, (msg.length < 512) ? msg.length : 512);
        else
            for (int i = 0; i < 128; i++) stim[i] = (float)(lrng() % 1000)/500.0f-1;

        step_fn(stim);

        // 每50步广播激素状态
        if (step - last_rp >= 50) {
            float hm=0; int hmi=0;
            for (int i=0;i<16;i++){hm+=H[i];if(H[i]>H[hmi])hmi=i;}
            hm/=16;
            float em=0; int emi=0;
            for (int i=0;i<8;i++){if(fabsf(E[i])>em){em=fabsf(E[i]);emi=i;}}
            printf("[AFFECT] H=%.3f[%d] E[%d]=%+.3f\n", hm, hmi, emi, E[emi]);
            last_rp = step;

            // 广播激素状态
            eipc_msg_t bcast = {.type=EIPC_HORMONE, .sender=e_sys_task_self(),
                                .code=0, .length=64, .data=(void*)H};
            e_sys_ipc_send(0, &bcast);
        }
        e_sys_task_yield();
    }
}
