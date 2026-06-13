// ============================================================
// EmotionOS - AffectiveCore
// 情感神经网络核：完整H(t+1), E(t+1)动力学 + 记忆驱动的突触雕刻
// 模拟"下丘脑-垂体-肾上腺"轴的硅基实现
//
// H(t+1) = sigmoid(A·H(t) + B·E(t) + C·S(t) + D·M(t) + noise_H)
// E(t+1) = tanh(W(t)·[H(t);E(t);S(t)] + noise_E)
// W(t) = 由MnemonicOrgan的STG活跃子图实时雕刻
// ============================================================
#pragma once
#include "../types.h"
#include "../memory/stg.h"
#include <cstdio>
#include <cstring>
#include <cmath>
#include <random>

namespace eos {
namespace affect {

constexpr int HORMONE_DIM   = 16;   // 激素向量维度
constexpr int EMOTION_DIM   = 8;    // 情绪向量维度
constexpr int STIMULUS_DIM  = 128;  // 外部刺激维度
constexpr int INPUT_TOTAL   = HORMONE_DIM + EMOTION_DIM + STIMULUS_DIM; // 152
constexpr int OUTPUT_DIM    = EMOTION_DIM;

// ---------- 激素名称（索引），用于调试/监控 ----------
enum HormoneIndex : int {
    H_DOPAMINE    = 0,   // 奖赏/愉悦
    H_CORTISOL    = 1,   // 压力/警觉
    H_SEROTONIN   = 2,   // 满足/稳定
    H_NORADRENALINE = 3, // 唤醒/兴奋
    H_OXYTOCIN    = 4,   // 信任/亲近
    H_ENDORPHIN   = 5,   // 镇痛/平静
    H_ADRENALINE  = 6,   // 爆发/恐惧
    H_MELATONIN   = 7,   // 抑制/睡眠倾向
    // 预留 8-15
};

// ---------- 情绪名称（索引）----------
enum EmotionIndex : int {
    E_JOY        = 0,
    E_SADNESS    = 1,
    E_ANGER      = 2,
    E_FEAR       = 3,
    E_SURPRISE   = 4,
    E_DISGUST    = 5,
    E_TRUST      = 6,
    E_ANTICIPATION = 7,
};

// ---------- AffectiveCore ----------
class AffectiveCore {
public:
    AffectiveCore() : step_(0), tick_(0) {
        std::random_device rd;
        rng_.seed(rd());
        init_matrices();
        init_hormone_emotion();
    }

    // ---- 初始化 ----
    void init_matrices() {
        std::uniform_real_distribution<float> d(-0.3f, 0.3f);
        // 矩阵A: 激素自关联 (16x16, 对角线0.7)
        for (int i = 0; i < HORMONE_DIM; i++) {
            for (int j = 0; j < HORMONE_DIM; j++) {
                A_[i][j] = (i == j) ? 0.7f : d(rng_) * 0.1f;
            }
        }
        // 矩阵B: 情绪→激素耦合 (16x8)
        for (int i = 0; i < HORMONE_DIM; i++)
            for (int j = 0; j < EMOTION_DIM; j++)
                B_[i][j] = d(rng_) * 0.2f;
        // 矩阵C: 刺激→激素 (16x128)
        for (int i = 0; i < HORMONE_DIM; i++)
            for (int j = 0; j < STIMULUS_DIM; j++)
                C_[i][j] = d(rng_) * 0.15f;
        // 矩阵D: 记忆回传→激素 (16x128) — 由STG短时活跃子图填充
        memset(D_, 0, sizeof(D_));
        // 矩阵W: 情绪权重 (8x152) — 由STG→LTL长期地形雕刻
        memset(W_, 0, sizeof(W_));
        init_baseline_W();
    }

    void init_baseline_W() {
        std::uniform_real_distribution<float> d(-0.1f, 0.1f);
        for (int i = 0; i < EMOTION_DIM; i++)
            for (int j = 0; j < INPUT_TOTAL; j++)
                W_[i][j] = d(rng_) * 0.5f;
        sculpting_active_ = true;
    }

    void init_hormone_emotion() {
        std::uniform_real_distribution<float> d(0.2f, 0.5f);
        for (int i = 0; i < HORMONE_DIM; i++) H_[i] = d(rng_);
        for (int i = 0; i < EMOTION_DIM; i++) E_[i] = 0.0f;
        // 默认中等唤醒
        H_[H_DOPAMINE] = 0.4f;
        H_[H_CORTISOL] = 0.3f;
        H_[H_SEROTONIN] = 0.5f;
        H_[H_NORADRENALINE] = 0.3f;
        copy_state_to_memory_buffer();
    }

    void copy_state_to_memory_buffer() {
        memcpy(last_H_, H_, sizeof(H_));
        memcpy(last_E_, E_, sizeof(E_));
    }

    void set_tick(uint64_t tick) { tick_ = tick; }

    // ---- 核心更新 ----
    // 输入: S = 外部刺激向量 (128-d, 归一化到 [-1,1])
    //       stg = STG指针（用于动态突触雕刻）
    // 输出: 更新H_(16-d), E_(8-d)
    void step(const float* S, const eos::memory::STG* stg) {
        std::uniform_real_distribution<float> 
            noise_h(-0.08f, 0.08f), noise_e(-0.05f, 0.05f);

        // === 动态突触雕刻 (每10步重新雕刻W(t)) ===
        if (sculpting_active_ && stg && step_ % 10 == 0) {
            sculpt_W_from_STG(stg);
        }

        // === 激素更新 H(t+1) ===
        // H(t+1) = sigmoid(A·H(t) + B·E(t) + C·S(t) + D·M(t) + noise)
        // 其中D·M(t)是记忆回传项，如果STG活跃节点补充了D_
        float next_H[HORMONE_DIM] = {0};
        for (int i = 0; i < HORMONE_DIM; i++) {
            // A·H(t)
            for (int j = 0; j < HORMONE_DIM; j++)
                next_H[i] += A_[i][j] * H_[j];
            // B·E(t)
            for (int j = 0; j < EMOTION_DIM; j++)
                next_H[i] += B_[i][j] * E_[j];
            // C·S(t)
            for (int j = 0; j < STIMULUS_DIM; j++)
                next_H[i] += C_[i][j] * S[j];
            // D·M(t) — 记忆回传项
            for (int j = 0; j < STIMULUS_DIM; j++)
                next_H[i] += D_[i][j] * S[j] * 0.3f;
            // 噪声
            next_H[i] += noise_h(rng_);
            // sigmoid + 衰减0.97 (代谢)
            H_[i] = 1.0f / (1.0f + expf(-next_H[i])) * 0.97f;
        }

        // === 情绪更新 E(t+1) ===
        // 构建输入向量 [H(t+1); E(t); S] (152-d)
        float combined[INPUT_TOTAL];
        memcpy(combined, H_, HORMONE_DIM * sizeof(float));
        memcpy(combined + HORMONE_DIM, last_E_, EMOTION_DIM * sizeof(float));
        memcpy(combined + HORMONE_DIM + EMOTION_DIM, S, STIMULUS_DIM * sizeof(float));

        for (int i = 0; i < EMOTION_DIM; i++) {
            float raw = 0;
            for (int j = 0; j < INPUT_TOTAL; j++)
                raw += W_[i][j] * combined[j];
            raw += noise_e(rng_);
            // 情绪着色：激素对情绪的反馈
            float h_feedback = (H_[H_DOPAMINE] - H_[H_CORTISOL]) * 0.2f;
            E_[i] = tanhf(raw + h_feedback);
        }

        // === 激素调节回路 ===
        // 阈值触发：高皮质醇→全局警觉广播
        hormone_threshold_check();

        // 代谢率微调：皮质醇高时加快代谢（负反馈）
        float metabolic_rate = 0.03f + H_[H_CORTISOL] * 0.05f;
        for (int i = 0; i < HORMONE_DIM; i++) {
            if (i == H_DOPAMINE) continue; // 多巴胺保留
            H_[i] *= (1.0f - metabolic_rate * 0.1f);
        }

        // 慢时间尺度：高血清素→降低全局噪声增益
        float serotonin_effect = H_[H_SEROTONIN] * 0.5f;
        noise_gain_ = 1.0f - serotonin_effect;

        // 保存状态用于下一次
        copy_state_to_memory_buffer();
        step_++;
    }

    // ---- 动态突触雕刻 ----
    // 从STG活跃子图生长出W(t)（情感核临时权重）
    void sculpt_W_from_STG(const eos::memory::STG* stg) {
        // 1. 收集活跃节点
        int count = stg->node_count();
        if (count < 2) return;

        // 2. 对每个情绪维度，从STG活跃节点的特征向量映射到W行
        //    W[e][j] = Σ(active_node_i的tag * hormone_profile * embedding[j])
        //    实质上是"记忆活动的瞬时投影"
        
        // 先保留一部分基础结构
        float new_W[EMOTION_DIM][INPUT_TOTAL];
        memcpy(new_W, W_, sizeof(new_W));

        int active_found = 0;
        for (int i = 0; i < count && i < 64; i++) {
            const auto* node = stg->get_node(i);
            if (!node || !node->active || node->salience < 0.3f) continue;
            active_found++;

            // 用这个节点的emotion向量和tag来雕刻W相应的行
            for (int e = 0; e < EMOTION_DIM; e++) {
                // 节点情绪→W[e]投影
                float emotion_weight = node->emotion[e] * 0.15f;
                float tag_boost = node->tag * 0.1f;
                
                // 映射到HORMONE_DIM区域 (0-15)
                for (int h = 0; h < HORMONE_DIM; h++) {
                    new_W[e][h] += emotion_weight * node->hormone[h] * 0.3f;
                }
                // 映射到EMOTION_DIM区域 (16-23)
                for (int em = 0; em < EMOTION_DIM; em++) {
                    new_W[e][HORMONE_DIM + em] += tag_boost * node->emotion[em] * 0.2f;
                }
                // 映射到STIMULUS_DIM区域 (24-151) — 从vec采样
                for (int k = 0; k < 16; k++) {
                    int s_idx = (i * 8 + k) % 128;
                    new_W[e][HORMONE_DIM + EMOTION_DIM + s_idx] += 
                        node->vec[s_idx] * emotion_weight * 0.1f;
                }
            }
        }

        if (active_found > 0) {
            // 归一化 + 防止发散
            for (int e = 0; e < EMOTION_DIM; e++) {
                float norm = 0;
                for (int j = 0; j < INPUT_TOTAL; j++) norm += new_W[e][j] * new_W[e][j];
                norm = sqrtf(norm);
                if (norm > 1.0f) {
                    float scale = 0.8f / norm;
                    for (int j = 0; j < INPUT_TOTAL; j++)
                        new_W[e][j] *= scale;
                }
                // 限幅
                for (int j = 0; j < INPUT_TOTAL; j++)
                    new_W[e][j] = std::max(-1.0f, std::min(1.0f, new_W[e][j]));
            }
            memcpy(W_, new_W, sizeof(W_));
        }
    }

    // ---- 激素阈值检查 ----
    int hormone_threshold_check() {
        int triggered = 0;
        // 皮质醇 > 0.75 → 警觉广播
        if (H_[H_CORTISOL] > 0.75f) {
            if (last_cortisol_alert_ == 0 || step_ - last_cortisol_alert_ > 20) {
                printf("[AFFECT] CORTISOL THRESHOLD H[1]=%.3f — ALERT\n", H_[H_CORTISOL]);
                last_cortisol_alert_ = step_;
                triggered = 1;
            }
        }
        // 多巴胺 > 0.8 → 奖赏峰值
        if (H_[H_DOPAMINE] > 0.8f) {
            if (last_dopamine_peak_ == 0 || step_ - last_dopamine_peak_ > 15) {
                printf("[AFFECT] DOPAMINE PEAK H[0]=%.3f — REWARD\n", H_[H_DOPAMINE]);
                last_dopamine_peak_ = step_;
                triggered |= 2;
            }
        }
        // 肾上腺素 > 0.8 → 恐惧/爆发
        if (H_[H_ADRENALINE] > 0.8f) {
            if (last_adrenaline_spike_ == 0 || step_ - last_adrenaline_spike_ > 15) {
                printf("[AFFECT] ADRENALINE SPIKE H[6]=%.3f — FIGHT/FLIGHT\n", H_[H_ADRENALINE]);
                last_adrenaline_spike_ = step_;
                triggered |= 4;
            }
        }
        return triggered;
    }

    // ---- 外部注入刺激 ----
    void inject_stimulus(const float* S, int count) {
        // S是128维向量
        // 储存到pending刺激缓冲区
        int n = (count < STIMULUS_DIM) ? count : STIMULUS_DIM;
        memcpy(pending_stimulus_, S, n * sizeof(float));
        has_pending_ = true;
    }

    bool consume_pending_stimulus(float* out_S) {
        if (!has_pending_) return false;
        memcpy(out_S, pending_stimulus_, STIMULUS_DIM * sizeof(float));
        has_pending_ = false;
        return true;
    }

    // ---- 记忆回传接口 ----
    // 记忆系统调用此函数，用STG活跃子图填充D矩阵
    void apply_memory_feedback(const float* memory_vec, float intensity) {
        // memory_vec是128-d张量向量
        // intensity是情感标签强度(-1~1)
        for (int i = 0; i < HORMONE_DIM; i++) {
            for (int j = 0; j < STIMULUS_DIM; j++) {
                float delta = memory_vec[j] * intensity * 0.01f;
                D_[i][j] = std::max(-0.5f, std::min(0.5f, D_[i][j] + delta));
            }
        }
        // 指数衰减D矩阵（记忆反馈会随时间消散，除非持续强化）
        for (int i = 0; i < HORMONE_DIM; i++)
            for (int j = 0; j < STIMULUS_DIM; j++)
                D_[i][j] *= 0.995f;
    }

    // ---- 获取内部状态 ----
    const float* hormones() const { return H_; }
    const float* emotions() const { return E_; }
    const float (*W() const)[INPUT_TOTAL] { return W_; }
    uint64_t step_count() const { return step_; }

    // ---- 打印状态 ----
    void print() const {
        float h_mean = 0;
        int h_max_idx = 0;
        for (int i = 0; i < HORMONE_DIM; i++) {
            h_mean += H_[i];
            if (H_[i] > H_[h_max_idx]) h_max_idx = i;
        }
        h_mean /= HORMONE_DIM;

        int e_max_idx = 0;
        float e_max_val = 0;
        for (int i = 0; i < EMOTION_DIM; i++) {
            if (fabsf(E_[i]) > e_max_val) {
                e_max_val = fabsf(E_[i]);
                e_max_idx = i;
            }
        }

        printf("[AFFECT] H=%.3f[%d] (DA=%.2f/CO=%.2f/SE=%.2f/NA=%.2f) "
               "E[%d]=%+.3f step=%llu | W_sculpt=%s\n",
               h_mean, h_max_idx,
               H_[H_DOPAMINE], H_[H_CORTISOL], H_[H_SEROTONIN], H_[H_NORADRENALINE],
               e_max_idx, E_[e_max_idx], step_,
               sculpting_active_ ? "active" : "static");

        // 情绪详细
        printf("  E: ");
        const char* e_names[] = {"Joy","Sad","Ang","Fear","Surp","Disg","Trst","Antc"};
        for (int i = 0; i < EMOTION_DIM; i++)
            printf("%s%+.2f ", e_names[i], E_[i]);
        printf("\n");
    }

    void enable_sculpting(bool en) { sculpting_active_ = en; }
    bool is_sculpting_active() const { return sculpting_active_; }
    float noise_gain() const { return noise_gain_; }

private:
    // 激素向量
    float H_[HORMONE_DIM];
    // 情绪向量
    float E_[EMOTION_DIM];
    float last_H_[HORMONE_DIM];
    float last_E_[EMOTION_DIM];

    // 权重矩阵
    float A_[HORMONE_DIM][HORMONE_DIM];      // 激素自关联
    float B_[HORMONE_DIM][EMOTION_DIM];       // 情绪→激素
    float C_[HORMONE_DIM][STIMULUS_DIM];      // 刺激→激素
    float D_[HORMONE_DIM][STIMULUS_DIM];      // 记忆回传
    float W_[EMOTION_DIM][INPUT_TOTAL];       // 雕刻权重 [H;E;S]→E

    // 刺激缓存
    float pending_stimulus_[STIMULUS_DIM];
    bool has_pending_ = false;

    // 状态
    uint64_t step_;
    uint64_t tick_;
    float noise_gain_ = 1.0f;
    bool sculpting_active_ = true;

    // 阈值触发去重
    uint64_t last_cortisol_alert_ = 0;
    uint64_t last_dopamine_peak_ = 0;
    uint64_t last_adrenaline_spike_ = 0;

    std::mt19937 rng_;
};

} // namespace affect
} // namespace eos
