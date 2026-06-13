#pragma once
#include <vector>
#include <array>
#include <cstdint>
#include <algorithm>
#include <random>

// ============================================================
// EmotionOS Phase 1 MVP - 基本类型定义
// ============================================================

// 固定维度向量
template <size_t N>
using Vec = std::array<float, N>;

// 16维激素向量
using HormoneVec = Vec<16>;

// 8维情绪向量
using EmotionVec = Vec<8>;

// 128维外部刺激向量
using StimulusVec = Vec<128>;

// 混沌随机数发生器
struct ChaosRng {
    std::mt19937_64 engine;
    
    ChaosRng(uint64_t seed = std::random_device{}())
        : engine(seed) {}
    
    // 生成[0,1)范围内的混沌噪声
    float noise() {
        std::uniform_real_distribution<float> dist(-0.1f, 0.1f);
        return dist(engine);
    }
    
    // 生成指定维度的混沌噪声向量
    template <size_t N>
    Vec<N> chaos_vec() {
        Vec<N> v{};
        for (auto& x : v) x = noise();
        return v;
    }
    
    // 生成高斯噪声
    template <size_t N>
    Vec<N> gaussian(float stddev = 0.05f) {
        Vec<N> v{};
        std::normal_distribution<float> dist(0.0f, stddev);
        for (auto& x : v) x = dist(engine);
        return v;
    }
};

// 激活函数
inline float sigmoid(float x) {
    return 1.0f / (1.0f + std::exp(-x));
}

inline float tanh_approx(float x) {
    return std::tanh(x);
}

// 向量逐元素操作
template <size_t N>
Vec<N> vec_add(const Vec<N>& a, const Vec<N>& b) {
    Vec<N> r{};
    for (size_t i = 0; i < N; i++) r[i] = a[i] + b[i];
    return r;
}

template <size_t N>
Vec<N> vec_sub(const Vec<N>& a, const Vec<N>& b) {
    Vec<N> r{};
    for (size_t i = 0; i < N; i++) r[i] = a[i] - b[i];
    return r;
}

template <size_t N>
Vec<N> vec_mul(const Vec<N>& a, const Vec<N>& b) {
    Vec<N> r{};
    for (size_t i = 0; i < N; i++) r[i] = a[i] * b[i];
    return r;
}

template <size_t N>
Vec<N> vec_scale(const Vec<N>& a, float s) {
    Vec<N> r{};
    for (size_t i = 0; i < N; i++) r[i] = a[i] * s;
    return r;
}

template <size_t N>
Vec<N> vec_sigmoid(const Vec<N>& a) {
    Vec<N> r{};
    for (size_t i = 0; i < N; i++) r[i] = sigmoid(a[i]);
    return r;
}

template <size_t N>
Vec<N> vec_tanh(const Vec<N>& a) {
    Vec<N> r{};
    for (size_t i = 0; i < N; i++) r[i] = tanh_approx(a[i]);
    return r;
}

template <size_t N>
Vec<N> vec_clamp(const Vec<N>& a, float lo, float hi) {
    Vec<N> r{};
    for (size_t i = 0; i < N; i++) r[i] = std::clamp(a[i], lo, hi);
    return r;
}

// 矩阵乘法：M x K * K x N -> M x N（简化版，仅用于小矩阵）
template <size_t M, size_t K, size_t N>
void mat_mul(const std::array<std::array<float, K>, M>& A,
             const std::array<std::array<float, N>, K>& B,
             std::array<std::array<float, N>, M>& C) {
    for (size_t i = 0; i < M; i++) {
        for (size_t j = 0; j < N; j++) {
            float sum = 0.0f;
            for (size_t k = 0; k < K; k++) {
                sum += A[i][k] * B[k][j];
            }
            C[i][j] = sum;
        }
    }
}

// 矩阵乘向量：M x N * N -> M
template <size_t M, size_t N>
Vec<M> mat_vec_mul(const std::array<std::array<float, N>, M>& A,
                   const Vec<N>& x) {
    Vec<M> r{};
    for (size_t i = 0; i < M; i++) {
        float sum = 0.0f;
        for (size_t j = 0; j < N; j++) {
            sum += A[i][j] * x[j];
        }
        r[i] = sum;
    }
    return r;
}

// 向量拼接：把两个向量拼成一个更大的向量
template <size_t N1, size_t N2, size_t NOut = N1 + N2>
Vec<N1 + N2> vec_concat(const Vec<N1>& a, const Vec<N2>& b) {
    Vec<N1 + N2> r{};
    for (size_t i = 0; i < N1; i++) r[i] = a[i];
    for (size_t i = 0; i < N2; i++) r[N1 + i] = b[i];
    return r;
}

// 向量拼接：三个向量拼接
template <size_t N1, size_t N2, size_t N3>
Vec<N1 + N2 + N3> vec_concat3(const Vec<N1>& a, const Vec<N2>& b, const Vec<N3>& c) {
    Vec<N1 + N2 + N3> r{};
    size_t offset = 0;
    for (size_t i = 0; i < N1; i++) r[offset++] = a[i];
    for (size_t i = 0; i < N2; i++) r[offset++] = b[i];
    for (size_t i = 0; i < N3; i++) r[offset++] = c[i];
    return r;
}
