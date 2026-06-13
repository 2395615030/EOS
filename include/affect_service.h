#pragma once
#include "emotionos.h"
#include "scheduler.h"
#include <cstdio>
#include <cmath>
#include <random>
#include <array>

// ============================================================
// EmotionOS - 情感核服务 (作为OS任务运行)
// ============================================================

constexpr size_t HORMONE_DIM = 16;
constexpr size_t EMOTION_DIM = 8;
constexpr size_t STIMULUS_DIM = 128;

using HormoneVec = std::array<float, HORMONE_DIM>;
using EmotionVec = std::array<float, EMOTION_DIM>;
using StimulusVec = std::array<float, STIMULUS_DIM>;

struct AffectService {
    HormoneVec H{};
    EmotionVec E{};
    std::mt19937_64 rng;
    uint64_t step_count = 0;
    
    // 权重 (简化)
    std::array<std::array<float, HORMONE_DIM>, HORMONE_DIM> A{};
    std::array<std::array<float, STIMULUS_DIM>, HORMONE_DIM> C{};
    
    AffectService() : rng(std::random_device{}()) {
        // 初始化权重
        std::uniform_real_distribution<float> dist(-0.3f, 0.3f);
        for (auto& row : A) for (auto& v : row) v = 0.0f;
        for (size_t i = 0; i < HORMONE_DIM; i++) {
            A[i][i] = 0.7f;
            for (size_t j = 0; j < HORMONE_DIM; j++) {
                if (i != j) A[i][j] = dist(rng) * 0.1f;
            }
        }
        for (auto& row : C) for (auto& v : row) v = dist(rng) * 0.15f;
        
        // 初始激素
        for (auto& h : H) h = 0.3f + dist(rng) * 0.1f;
    }
    
    void step(const StimulusVec& S) {
        std::uniform_real_distribution<float> noise_dist(-0.1f, 0.1f);
        
        // 激素更新 (简化混沌)
        for (size_t i = 0; i < HORMONE_DIM; i++) {
            float ah = 0.0f;
            for (size_t j = 0; j < HORMONE_DIM; j++) ah += A[i][j] * H[j];
            
            float cs = 0.0f;
            for (size_t j = 0; j < STIMULUS_DIM; j++) cs += C[i][j] * S[j];
            
            float raw = ah + cs + noise_dist(rng);
            H[i] = 1.0f / (1.0f + std::exp(-raw));
            H[i] *= 0.97f; // 衰减
        }
        
        // 情绪更新 (简化)
        float h_avg = 0.0f;
        for (auto& h : H) h_avg += h;
        h_avg /= HORMONE_DIM;
        
        float phase = step_count * 0.1f;
        for (size_t i = 0; i < EMOTION_DIM; i++) {
            float bias = h_avg * 0.5f + noise_dist(rng) * 0.1f;
            E[i] = std::tanh(bias + 0.3f * std::sin(phase + i * 0.5f));
        }
        
        step_count++;
    }
    
    void print() const {
        float h_mean = 0.0f;
        size_t h_max_idx = 0;
        for (size_t i = 0; i < HORMONE_DIM; i++) {
            h_mean += H[i];
            if (H[i] > H[h_max_idx]) h_max_idx = i;
        }
        h_mean /= HORMONE_DIM;
        
        size_t e_max_idx = 0;
        for (size_t i = 0; i < EMOTION_DIM; i++) {
            if (std::abs(E[i]) > std::abs(E[e_max_idx])) e_max_idx = i;
        }
        
        printf("[AFFECT] H=%.3f[%zu] E[%zu]=%+.3f step=%llu\n",
               h_mean, h_max_idx, e_max_idx, E[e_max_idx], step_count);
    }
};
