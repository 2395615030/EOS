// ============================================================
// EmotionOS - ???? (?????)
// ?? MnemonicOrgan ????? IPC ?? 2 ????
// ============================================================
#include "../include/emotionos/types.h"
#include "../include/emotionos/syscall.h"
#include "../include/emotionos/memory/mnemonic.h"
#include <cstdio>
#include <cstring>

static eos::memory::MnemonicOrgan g_mem;
static float g_last_hormone[16] = {0};
static float g_last_emotion[8] = {0};
static uint64_t g_tick = 0;
static int g_write_count = 0;

// ???
void memory_component_init() {
    printf("[MEM] Mnemonic Organ v0.1 init.\n");
    g_mem.debug_print();
}

// ????IPC???
static void handle_write(const eipc_msg_t* msg, const float* hormone, const float* emotion) {
    if (!msg->data || msg->length < 4) return;
    
    int count = msg->length / 4;
    float vec[128] = {0};
    int n = (count < 128) ? count : 128;
    memcpy(vec, msg->data, n * sizeof(float));
    
    int idx = g_mem.write(vec, hormone, emotion, msg->fvalue, g_tick);
    g_write_count++;
    
    // ??????????? -> ?????
    float emax = 0;
    for (int i = 0; i < 8; i++) if (fabsf(emotion[i]) > emax) emax = fabsf(emotion[i]);
    if (emax > 0.7f) {
        g_mem.flashbulb_memory(idx, emotion);
        printf("[MEM] Flashbulb! idx=%d |emax|=%.2f\n", idx, emax);
    }
}

// ???????IPC???
static void handle_recall(const eipc_msg_t* msg, const float* hormone) {
    float query[128] = {0};
    if (msg->data && msg->length >= 4) {
        int n = (msg->length / 4 < 128) ? msg->length / 4 : 128;
        memcpy(query, msg->data, n * sizeof(float));
    }
    
    auto result = g_mem.recall(query, hormone);
    
    // ?????????
    // ??IPC????
    printf("[MEM] Recall: %d STG nodes, LTL attr=%d score=%.3f\n",
           result.stg_count, result.ltl_attractor_idx, result.ltl_score);
    
    if (result.stg_count > 0) {
        printf("[MEM] Top: ");
        for (int i = 0; i < result.stg_count && i < 3; i++) {
            const auto* node = g_mem.stg_node_count() > result.stg_indices[i] ? 
                nullptr : nullptr; // can't access STGNode directly here
            printf("[%d]s=%.3f ", result.stg_indices[i], result.stg_scores[i]);
        }
        printf("\n");
    }
}

// ??????
void memory_component_update_temperature(const float* hormone) {
    memcpy(g_last_hormone, hormone, sizeof(float) * 16);
    g_mem.update_temperatures(hormone);
}

// ???
void memory_component_run(uint64_t tick) {
    g_tick = tick;
    g_mem.set_tick(tick);
    
    // ???????
    static int dbg = 0;
    if (++dbg % 30 == 0) {
        printf("[MEM] writes=%d ", g_write_count);
        g_mem.debug_print();
    }
}

// ????/????
float* memory_get_hormone() { return g_last_hormone; }
float* memory_get_emotion() { return g_last_emotion; }
void memory_set_emotion(const float* e) { memcpy(g_last_emotion, e, sizeof(float) * 8); }

// ---- ??entry?? (?main.cpp??) ----
void m_entry(void*) {
    printf("[MEMORY] Mnemonic Organ service started.\n");
    memory_component_init();
    e_sys_ipc_bind(2);
    
    // ????????
    float h[16] = {0.4f};
    float e[8] = {0};
    
    while (1) {
        eipc_msg_t m;
        int r = e_sys_ipc_recv(&m, 200);
        
        // ????????
        int hc = 16, ec = 8;
        e_sys_affect_get(h, &hc, e, &ec);
        memcpy(g_last_hormone, h, sizeof(float) * 16);
        memcpy(g_last_emotion, e, sizeof(float) * 8);
        
        if (r == 0) {
            if (m.type == EIPC_MEMORY) {
                handle_write(&m, h, e);
            } else if (m.type == EIPC_USER && m.code == 0x10) {
                handle_recall(&m, h);
            }
        }
        
        memory_component_run(g_tick + 1);
        e_sys_task_yield();
    }
}
