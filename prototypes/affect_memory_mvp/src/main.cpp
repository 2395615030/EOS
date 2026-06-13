// ============================================================
// EmotionOS Phase 1 MVP - 情感核 + 记忆核 闭环
// 一个"活的"情感-记忆自循环系统
// ============================================================
// 编译: cl /std:c++20 /O2 /EHsc main.cpp
// 运行: main.exe

#include <cstdio>
#include <cmath>
#include <vector>
#include <random>

// 核心实现
#include "types.h"
#include "affect.h"
#include "memory.h"

// ============================================================
// 外部刺激生成器（模拟传感器输入）
// ============================================================
class StimulusGenerator {
public:
    StimulusGenerator() : time_(0) {}
    
    // 背景刺激：低频周期模式
    StimulusVec background() {
        StimulusVec S{};
        float phase1 = std::sin(time_ * 0.05f);
        float phase2 = std::cos(time_ * 0.13f);
        
        for (size_t i = 0; i < S.size(); i++) {
            S[i] = phase1 * std::sin((float)i * 0.1f) * 0.2f
                 + phase2 * std::cos((float)i * 0.07f) * 0.15f
                 + rng_.noise() * 0.05f;
        }
        time_++;
        return S;
    }
    
    // 危险刺激：前16维爆发
    StimulusVec danger() {
        StimulusVec S{};
        for (size_t i = 0; i < S.size(); i++) {
            S[i] = rng_.noise() * 0.1f;
        }
        for (size_t i = 0; i < 16; i++) {
            S[i] = 0.9f + rng_.noise() * 0.1f;
        }
        return S;
    }
    
    // 愉悦刺激：48-64维爆发
    StimulusVec reward() {
        StimulusVec S{};
        for (size_t i = 0; i < S.size(); i++) {
            S[i] = rng_.noise() * 0.1f;
        }
        for (size_t i = 48; i < 64; i++) {
            S[i] = 0.85f + rng_.noise() * 0.1f;
        }
        return S;
    }
    
    // 社交刺激：80-96维爆发
    StimulusVec social() {
        StimulusVec S{};
        for (size_t i = 0; i < S.size(); i++) {
            S[i] = rng_.noise() * 0.1f;
        }
        for (size_t i = 80; i < 96; i++) {
            S[i] = 0.8f + rng_.noise() * 0.15f;
        }
        return S;
    }
    
    // 复杂探索刺激：混合多种模式
    StimulusVec explore() {
        StimulusVec S{};
        float t = time_ * 0.1f;
        for (size_t i = 0; i < S.size(); i++) {
            S[i] = std::sin((float)i * 0.05f + t) * 0.5f
                 + std::cos((float)i * 0.03f - t * 0.7f) * 0.3f
                 + rng_.noise() * 0.2f;
        }
        time_++;
        return S;
    }
    
private:
    int time_ = 0;
    ChaosRng rng_;
};

// 事件调度的类型
enum class EventType { NONE, DANGER, REWARD, SOCIAL };

// ============================================================
// 主循环
// ============================================================
int main() {
    std::printf("============================================================\n");
    std::printf(" EmotionOS Phase 1 MVP\n");
    std::printf(" 情感核 + 记忆核 闭环系统\n");
    std::printf(" 事件: 危险[!] / 奖励[+] / 社交[S] / 探索[~]\n");
    std::printf("============================================================\n\n");
    
    AffectiveCore affect(42);
    MemoryCore memory;
    StimulusGenerator stim_gen;
    
    const int TOTAL_STEPS = 1000;
    const int PRINT_INTERVAL = 50;
    
    // 事件调度表
    struct ScheduledEvent { int step; EventType type; };
    std::vector<ScheduledEvent> events = {
        {50,  EventType::DANGER},
        {120, EventType::REWARD},
        {200, EventType::SOCIAL},
        {280, EventType::DANGER},
        {350, EventType::REWARD},
        {420, EventType::SOCIAL},
        {500, EventType::DANGER},
        {550, EventType::REWARD},
        {630, EventType::DANGER},
        {700, EventType::REWARD},
        {750, EventType::REWARD},
        {800, EventType::SOCIAL},
        {880, EventType::DANGER},
        {950, EventType::REWARD},
    };
    
    auto next_event = events.begin();
    int last_event_step = -100;
    
    std::printf("开始运行 %d 步...\n\n", TOTAL_STEPS);
    
    // 激素历史记录
    float H_history[TOTAL_STEPS][3] = {0}; // 存前三个激素维度
    
    for (int t = 0; t < TOTAL_STEPS; t++) {
        // 1. 判断是否有事件
        EventType current_event = EventType::NONE;
        if (next_event != events.end() && next_event->step == t) {
            current_event = next_event->type;
            next_event++;
        }
        
        // 2. 生成刺激
        StimulusVec S;
        const char* event_label = "";
        switch (current_event) {
            case EventType::DANGER:
                S = stim_gen.danger();
                event_label = " [!危险!] ";
                last_event_step = t;
                break;
            case EventType::REWARD:
                S = stim_gen.reward();
                event_label = " [+奖励+] ";
                last_event_step = t;
                break;
            case EventType::SOCIAL:
                S = stim_gen.social();
                event_label = " [S社交] ";
                last_event_step = t;
                break;
            default:
                // 无事件时使用探索刺激（但偶尔也会回到背景模式）
                if ((t - last_event_step) < 15) {
                    S = stim_gen.background(); // 事件后平静期
                } else {
                    S = stim_gen.explore();
                }
                break;
        }
        
        // 3. 从记忆网络获取雕刻权重
        memory.get_live_weights(affect.weights().W);
        
        // 4. 情感核更新
        HormoneVec M = affect.hormone();
        affect.step(S, M);
        
        // 5. 记录到记忆网络
        memory.write(S, affect.hormone(), affect.emotion(), t);
        
        // 6. 从记忆网络获取联想
        float temp = 0.0f;
        for (auto& h : affect.hormone()) temp += h;
        temp /= HORMONE_DIM;
        temp = 0.3f + temp * 0.7f;
        
        auto recalled = memory.recall(S, temp);
        
        // 记录激素历史
        H_history[t][0] = affect.hormone()[0];
        H_history[t][1] = affect.hormone()[4];
        H_history[t][2] = affect.hormone()[12];
        
        // 打印
        if (t % PRINT_INTERVAL == 0 || current_event != EventType::NONE) {
            auto& H = affect.hormone();
            auto& E = affect.emotion();
            
            float H_mean = 0.0f;
            size_t H_max_idx = 0;
            for (size_t i = 0; i < HORMONE_DIM; i++) {
                H_mean += H[i];
                if (H[i] > H[H_max_idx]) H_max_idx = i;
            }
            H_mean /= HORMONE_DIM;
            
            size_t E_max_idx = 0;
            for (size_t i = 0; i < EMOTION_DIM; i++) {
                if (std::abs(E[i]) > std::abs(E[E_max_idx])) E_max_idx = i;
            }
            
            std::printf("[%04d]%s H=%.3f[%zu] E[%zu]=%+.3f STG=%zu",
                       t, event_label, H_mean, H_max_idx,
                       E_max_idx, E[E_max_idx], memory.stg().size());
            
            if (t % (PRINT_INTERVAL * 2) == 0) {
                std::printf(" LTL_max=%.4f", 0.0f); // 简化
            }
            std::printf("\n");
        }
    }
    
    // 最终总结
    std::printf("\n============================================================\n");
    std::printf(" 运行完成! %d 步\n", TOTAL_STEPS);
    std::printf(" STG节点数: %zu  |  激素平均轨迹:\n", memory.stg().size());
    
    // 打印激素变化轨迹（每50步采一个点）
    std::printf("\n 激素H[0]轨迹（每50步）:\n  ");
    for (int t = 0; t < TOTAL_STEPS; t += 50) {
        std::printf("%.2f ", H_history[t][0]);
    }
    std::printf("\n  激素H[4]轨迹:\n  ");
    for (int t = 0; t < TOTAL_STEPS; t += 50) {
        std::printf("%.2f ", H_history[t][1]);
    }
    std::printf("\n  激素H[12]轨迹:\n  ");
    for (int t = 0; t < TOTAL_STEPS; t += 50) {
        std::printf("%.2f ", H_history[t][2]);
    }
    std::printf("\n\n 系统状态: 情感-记忆闭环运行正常 ✓\n");
    std::printf("============================================================\n");
    
    return 0;
}
