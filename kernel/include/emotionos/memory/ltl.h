// ============================================================
// EmotionOS - LTL: Long-Term Landscape
// ?????? - ??????? + ????
// ============================================================
#pragma once
#include "../types.h"
#include <vector>
#include <cmath>
#include <algorithm>
#include <cstdint>
#include <cstring>
#include <random>

namespace eos {
namespace memory {

// ---------- LTL?? ----------
constexpr int LTL_BASIN_SIZE    = 8;     // ?????????????
constexpr int LTL_MAX_ATTRACTORS = 256;  // ???????
constexpr int LTL_VEC_DIM       = 128;   // ??????STG???
constexpr int LTL_EMBED_DIM     = 64;    // ????

// ---------- ??? (Attractor) ----------
struct Attractor {
    float center[LTL_VEC_DIM];       // ???????????
    float embed[LTL_EMBED_DIM];      // ????????????
    float emotion_label[8];          // ???????
    float hormone_profile[16];       // ????????
    float depth;                     // ????? (0~1, ?=??)
    float radius;                    // ????
    float consolidation;             // ??? (0~1, ??????)
    uint64_t formation_tick;         // ????
    uint64_t last_activated;         // ??????
    int stg_node_count;              // ???STG????
    uint16_t stg_nodes[64];          // ???STG????
    bool active;
    
    Attractor() { clear(); }
    void clear() {
        memset(center, 0, sizeof(center));
        memset(embed, 0, sizeof(embed));
        memset(emotion_label, 0, sizeof(emotion_label));
        memset(hormone_profile, 0, sizeof(hormone_profile));
        depth = 0.3f; radius = 0.2f; consolidation = 0.0f;
        formation_tick = 0; last_activated = 0;
        stg_node_count = 0; active = false;
    }
};

// ---------- LTL?? ----------
class LTL {
public:
    LTL() : attractor_count_(0), tick_(0) {
        std::random_device rd;
        rng_.seed(rd());
    }
    
    // ---- ?? ----
    // ?STG??????????/?????
    void consolidate(const float* stg_vectors, const int* stg_indices, 
                     int stg_count, const float* hormone, uint64_t tick) {
        tick_ = tick;
        if (stg_count == 0) return;
        
        // ???STG??????????
        for (int si = 0; si < stg_count; si++) {
            const float* vec = stg_vectors + si * LTL_VEC_DIM;
            
            int nearest = -1;
            float min_dist = 2.0f;
            for (int a = 0; a < attractor_count_; a++) {
                if (!attractors_[a].active) continue;
                float d = euclidean_dist(vec, attractors_[a].center);
                if (d < min_dist) { min_dist = d; nearest = a; }
            }
            
            const float FORMATION_THRESHOLD = 0.35f;  // ????
            const float STRENGTHEN_THRESHOLD = 0.25f; // ????
            
            if (nearest >= 0 && min_dist < FORMATION_THRESHOLD) {
                auto& a = attractors_[nearest];
                // ????????
                float rate = 0.1f * (1.0f - a.consolidation);
                for (int i = 0; i < LTL_VEC_DIM; i++)
                    a.center[i] += (vec[i] - a.center[i]) * rate;
                a.depth = std::min(1.0f, a.depth + 0.02f);
                a.consolidation = std::min(1.0f, a.consolidation + 0.05f);
                a.last_activated = tick;
                
                // ?????STG??
                if (a.stg_node_count < 64 && stg_indices) {
                    a.stg_nodes[a.stg_node_count++] = stg_indices[si];
                }
                
                // ??????
                for (int i = 0; i < 16; i++) {
                    a.hormone_profile[i] = a.hormone_profile[i] * 0.9f + hormone[i] * 0.1f;
                }
            } else if (min_dist >= FORMATION_THRESHOLD && attractor_count_ < LTL_MAX_ATTRACTORS) { 
                // ???????
                int idx = attractor_count_++;
                auto& a = attractors_[idx];
                a.active = true;
                memcpy(a.center, vec, sizeof(float) * LTL_VEC_DIM);
                compute_embedding(a.center, a.embed);
                a.depth = 0.5f;
                a.radius = 0.25f;
                a.consolidation = 0.1f;
                a.formation_tick = tick;
                a.last_activated = tick;
                memcpy(a.hormone_profile, hormone, sizeof(float) * 16);
                if (stg_indices && a.stg_node_count < 64) {
                    a.stg_nodes[a.stg_node_count++] = stg_indices[si];
                }
            }
        }
    }
    
    // ---- ?????????????? ----
    // ?????? + ?????????????
    // temperature: 0=????, 1=????, >1=????
    int sample(const float* query, float temperature,
               float* out_vec, float* out_emotion, float* out_score) {
        if (attractor_count_ == 0) return -1;
        
        // 1. ???????
        int nearest = -1;
        float min_dist = 2.0f;
        for (int a = 0; a < attractor_count_; a++) {
            if (!attractors_[a].active) continue;
            float d = euclidean_dist(query, attractors_[a].center);
            if (d < min_dist) { min_dist = d; nearest = a; }
        }
        if (nearest < 0) return -1;
        
        auto& attr = attractors_[nearest];
        attr.last_activated = tick_;
        
        // 2. ??????????
        if (temperature < 0.3f) {
            // ??????????????
            memcpy(out_vec, attr.center, sizeof(float) * LTL_VEC_DIM);
            memcpy(out_emotion, attr.emotion_label, sizeof(float) * 8);
            *out_score = attr.depth * (1.0f - min_dist);
            
        } else if (temperature < 0.7f) {
            // ??????? + ???
            std::normal_distribution<float> noise(0, temperature * 0.2f);
            for (int i = 0; i < LTL_VEC_DIM; i++) {
                out_vec[i] = attr.center[i] + noise(rng_);
                out_vec[i] = std::max(-1.0f, std::min(1.0f, out_vec[i]));
            }
            memcpy(out_emotion, attr.emotion_label, sizeof(float) * 8);
            *out_score = attr.depth * (1.0f - min_dist) * 0.8f;
            
        } else {
            // ?????????????????
            // ??? -> ??????????????
            std::normal_distribution<float> drift(0, temperature * 0.4f);
            for (int i = 0; i < LTL_VEC_DIM; i++) {
                out_vec[i] = attr.center[i] + drift(rng_);
                out_vec[i] = std::max(-1.0f, std::min(1.0f, out_vec[i]));
            }
            memcpy(out_emotion, attr.emotion_label, sizeof(float) * 8);
            // ??????
            for (int i = 0; i < 8; i++) {
                out_emotion[i] += (float)(rng_() % 200 - 100) / 500.0f;
                out_emotion[i] = std::max(-1.0f, std::min(1.0f, out_emotion[i]));
            }
            *out_score = attr.depth * 0.5f;
        }
        
        return nearest;
    }
    
    // ---- ??????????? ----
    // ?????? -> ????
    void flashbulb_consolidate(int attractor_idx) {
        if (attractor_idx < 0 || attractor_idx >= attractor_count_) return;
        if (!attractors_[attractor_idx].active) return;
        
        auto& a = attractors_[attractor_idx];
        a.consolidation = std::min(1.0f, a.consolidation + 0.3f);
        a.depth = std::min(1.0f, a.depth + 0.15f);
        // ??????????
        for (int i = 0; i < a.stg_node_count; i++) {
            // ????STG?????
            // ??????STG.update_temperatures????
        }
    }
    
    // ---- ?????????? ----
    void forget(float global_temperature) {
        for (int a = 0; a < attractor_count_; a++) {
            if (!attractors_[a].active) continue;
            
            // ??? + ?????? = ???
            uint64_t age = tick_ - attractors_[a].last_activated;
            float erosion = global_temperature * 0.001f * (1.0f - attractors_[a].consolidation);
            
            // ???????
            attractors_[a].depth -= erosion;
            attractors_[a].radius *= (1.0f + erosion * 0.1f); // ???? = ??
            
            // ???????0???
            if (attractors_[a].depth < 0.01f) {
                attractors_[a].active = false;
            }
            
            // ?????????????????
            if (age > 10000 && attractors_[a].consolidation < 0.3f) {
                attractors_[a].depth *= 0.995f;
            }
        }
    }
    
    // ---- ?????? ----
    void set_emotion_label(int attractor_idx, const float* emotion) {
        if (attractor_idx < 0 || attractor_idx >= attractor_count_) return;
        if (!attractors_[attractor_idx].active) return;
        memcpy(attractors_[attractor_idx].emotion_label, emotion, sizeof(float) * 8);
    }
    
    // ---- ?? ----
    int attractor_count() const { return attractor_count_; }
    const Attractor* get_attractor(int idx) const {
        if (idx < 0 || idx >= attractor_count_ || !attractors_[idx].active) return nullptr;
        return &attractors_[idx];
    }

    void set_tick(uint64_t tick) { tick_ = tick; }
    
    // ---- ???????? ----
    int find_by_emotion(const float* emotion_pattern, float threshold) {
        int best = -1;
        float best_sim = -2.0f;
        for (int a = 0; a < attractor_count_; a++) {
            if (!attractors_[a].active) continue;
            float sim = 0;
            for (int i = 0; i < 8; i++) sim += attractors_[a].emotion_label[i] * emotion_pattern[i];
            if (sim > best_sim) { best_sim = sim; best = a; }
        }
        return (best_sim > threshold) ? best : -1;
    }

private:
    Attractor attractors_[LTL_MAX_ATTRACTORS];
    int attractor_count_;
    uint64_t tick_;
    std::mt19937 rng_;
    
    static float euclidean_dist(const float* a, const float* b) {
        float d = 0;
        for (int i = 0; i < LTL_VEC_DIM; i++) {
            float diff = a[i] - b[i];
            d += diff * diff;
        }
        return sqrtf(d);
    }
    
    // ??????????????
    static void compute_embedding(const float* vec, float* embed) {
        // ????????
        int step = LTL_VEC_DIM / LTL_EMBED_DIM;
        for (int i = 0; i < LTL_EMBED_DIM; i++) {
            float s = 0;
            int start = i * step;
            int end = std::min(start + step, LTL_VEC_DIM);
            for (int j = start; j < end; j++) s += vec[j];
            embed[i] = s / (end - start);
        }
    }
};

} // namespace memory
} // namespace eos






