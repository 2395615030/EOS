#pragma once
#include "types.h"
#include <cstdio>

// ============================================================
// 情感神经网络 (Affective Core)
// 模拟"下丘脑-垂体-肾上腺"轴的硅基实现
// ============================================================

// 激素向量维度配置
constexpr size_t HORMONE_DIM = 16;
constexpr size_t EMOTION_DIM = 8;
constexpr size_t STIMULUS_DIM = 128;

// 情感核状态
struct AffectState {
    HormoneVec H{};      // 激素向量
    EmotionVec E{};      // 情绪向量
    float time = 0.0f;   // 内部时间
};

// 情感核权重矩阵（由记忆网络雕刻，这里是固定初始化版MVP）
struct AffectWeights {
    // A: 16x16 激素自耦合
    std::array<std::array<float, HORMONE_DIM>, HORMONE_DIM> A{};
    // B: 16x8 情绪->激素映射
    std::array<std::array<float, EMOTION_DIM>, HORMONE_DIM> B{};
    // C: 16x128 刺激->激素映射
    std::array<std::array<float, STIMULUS_DIM>, HORMONE_DIM> C{};
    // D: 16x16 记忆->激素映射
    std::array<std::array<float, HORMONE_DIM>, HORMONE_DIM> D{};
    // W: 8x(128+16+8)=8x152 映射到情绪
    std::array<std::array<float, STIMULUS_DIM + HORMONE_DIM + EMOTION_DIM>, EMOTION_DIM> W{};
    
    // 激素衰减率
    HormoneVec decay{};
    
    // 用确定性种子初始化
    void init(uint64_t seed = 42);
};

// 情感核
class AffectiveCore {
public:
    AffectiveCore(uint64_t seed = 42);
    
    // 单步更新
    // 输入: 外部刺激S, 记忆回传M
    // 输出: 更新后的激素H和情绪E
    void step(const StimulusVec& S, const HormoneVec& M);
    
    // 获取当前状态
    const AffectState& state() const { return state_; }
    HormoneVec& hormone() { return state_.H; }
    EmotionVec& emotion() { return state_.E; }
    
    // 获取权重（供记忆网络雕刻）
    AffectWeights& weights() { return weights_; }
    
    // 打印当前激素/情绪摘要
    void print_summary(int step) const;
    
private:
    AffectState state_;
    AffectWeights weights_;
    ChaosRng rng_;
};

// 情感核权重初始化
inline void AffectWeights::init(uint64_t seed) {
    std::mt19937 gen(seed);
    std::uniform_real_distribution<float> dist(-0.5f, 0.5f);
    std::uniform_real_distribution<float> decay_dist(0.95f, 0.99f);
    
    // 初始化A(自耦合)：对角占优，弱非对角耦合
    for (auto& row : A)
        for (auto& v : row) v = 0.0f;
    for (size_t i = 0; i < HORMONE_DIM; i++) {
        A[i][i] = 0.8f;  // 自耦合强
        for (size_t j = 0; j < HORMONE_DIM; j++) {
            if (i != j) A[i][j] = dist(gen) * 0.1f;  // 弱交叉耦合
        }
    }
    
    for (auto& row : B) for (auto& v : row) v = dist(gen) * 0.3f;
    for (auto& row : C) for (auto& v : row) v = dist(gen) * 0.2f;
    for (auto& row : D) for (auto& v : row) v = dist(gen) * 0.3f;
    for (auto& row : W) for (auto& v : row) v = dist(gen) * 0.5f;
    
    for (auto& d : decay) d = decay_dist(gen);
}

inline AffectiveCore::AffectiveCore(uint64_t seed)
    : rng_(seed) 
{
    weights_.init(seed);
    
    // 初始激素：随机但温和
    for (auto& h : state_.H) h = 0.3f + rng_.noise() * 0.1f;
    // 初始情绪：中性
    for (auto& e : state_.E) e = 0.0f;
}

inline void AffectiveCore::step(const StimulusVec& S, const HormoneVec& M) {
    auto& H = state_.H;
    auto& E = state_.E;
    auto& W = weights_;
    
    // ===== 1. 激素混沌更新 =====
    // H(t+1) = sigmoid(A*H + B*E + C*S + D*M + noise)
    auto AH = mat_vec_mul<HORMONE_DIM, HORMONE_DIM>(W.A, H);
    auto BE = mat_vec_mul<HORMONE_DIM, EMOTION_DIM>(W.B, E);
    auto CS = mat_vec_mul<HORMONE_DIM, STIMULUS_DIM>(W.C, S);
    auto DM = mat_vec_mul<HORMONE_DIM, HORMONE_DIM>(W.D, M);
    
    auto noise = rng_.chaos_vec<HORMONE_DIM>();
    
    auto H_raw = vec_add(vec_add(vec_add(AH, BE), vec_add(CS, DM)), noise);
    H = vec_sigmoid(H_raw);
    
    // 激素指数衰减
    H = vec_mul(H, W.decay);
    // 保持在[0,1]
    H = vec_clamp(H, 0.0f, 1.0f);
    
    // ===== 2. 情绪映射 =====
    // E(t+1) = tanh(W * concat(S, H, E) + noise)
    auto SHE = vec_concat3(S, H, E);
    auto E_raw = mat_vec_mul<EMOTION_DIM, STIMULUS_DIM + HORMONE_DIM + EMOTION_DIM>(W.W, SHE);
    auto E_noise = rng_.gaussian<EMOTION_DIM>(0.02f);
    E_raw = vec_add(E_raw, E_noise);
    E = vec_tanh(E_raw);
    
    state_.time += 1.0f;
}

inline void AffectiveCore::print_summary(int step) const {
    auto& H = state_.H;
    auto& E = state_.E;
    
    // 激素摘要：平均值 + 最强激素
    float H_mean = 0.0f;
    size_t H_max_idx = 0;
    for (size_t i = 0; i < HORMONE_DIM; i++) {
        H_mean += H[i];
        if (H[i] > H[H_max_idx]) H_max_idx = i;
    }
    H_mean /= HORMONE_DIM;
    
    // 情绪摘要：最强的情绪维度
    size_t E_max_idx = 0;
    for (size_t i = 0; i < EMOTION_DIM; i++) {
        if (std::abs(E[i]) > std::abs(E[E_max_idx])) E_max_idx = i;
    }
    
    std::printf("[%04d] H_mean=%.4f H_max[%zu]=%.4f E_dom[%zu]=%+.4f\n",
                step, H_mean, H_max_idx, H[H_max_idx],
                E_max_idx, E[E_max_idx]);
}
