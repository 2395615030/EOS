// ============================================================
// EmotionOS - Mnemonic Organ (????)
// ?????STG????? + LTL??????
// ============================================================
#pragma once
#include "../types.h"
#include "stg.h"
#include "ltl.h"
#include <cstdio>
#include <cstring>
#include <cmath>

namespace eos {
namespace memory {

constexpr int MEM_CONSOLIDATION_INTERVAL = 10;
constexpr int MEM_DECAY_INTERVAL         = 10;
constexpr int MEM_RECALL_TOP_K           = 6;
constexpr int MEM_VEC_DIM                = 128;

struct RecallResult {
    int    stg_indices[MEM_RECALL_TOP_K];
    float  stg_scores[MEM_RECALL_TOP_K];
    int    stg_count;
    float  ltl_vec[MEM_VEC_DIM];
    int    ltl_attractor_idx;
    float  ltl_score;
    float  ltl_emotion[8];
};

class MnemonicOrgan {
public:
    MnemonicOrgan() : tick_(0), tick_since_consolidation_(0), tick_since_decay_(0) {}

    // ???tick???????????????
    void consolidate_tick(const float* hormone) { 
        tick_since_consolidation_++;
        tick_since_decay_++;
        if (tick_since_consolidation_ >= MEM_CONSOLIDATION_INTERVAL) {
            consolidate_to_ltl(hormone);
            tick_since_consolidation_ = 0;
        }
        if (tick_since_decay_ >= MEM_DECAY_INTERVAL) {
            decay();
            tick_since_decay_ = 0;
        }
    }

    int write(const float* vec, const float* hormone, const float* emotion,
              float tag, uint64_t tick) {
        tick_ = tick;
        return stg_.write(vec, hormone, emotion, tag, tick);
    }

    RecallResult recall(const float* query, const float* hormone) {
        RecallResult result;
        memset(&result, 0, sizeof(result));
        if (!query) return result;

        result.stg_count = stg_.recall(query, hormone,
            result.stg_indices, result.stg_scores, MEM_RECALL_TOP_K);

        float temp = hormone ? avg_hormone(hormone) : 0.5f;
        float ltl_out[MEM_VEC_DIM] = {0};
        float ltl_emotion[8] = {0};

        result.ltl_attractor_idx = ltl_.sample(query, temp,
            ltl_out, ltl_emotion, &result.ltl_score);

        if (result.ltl_attractor_idx >= 0) {
            memcpy(result.ltl_vec, ltl_out, sizeof(float) * MEM_VEC_DIM);
            memcpy(result.ltl_emotion, ltl_emotion, sizeof(float) * 8);
        }

        // ????????
        if (hormone && result.stg_count > 0) {
            float h_mean = avg_hormone(hormone);
            if (h_mean > 0.6f) {
                for (int i = 0; i < result.stg_count; i++) {
                    const auto* node = stg_.get_node(result.stg_indices[i]);
                    if (node && node->tag > 0.3f) result.stg_scores[i] *= 1.2f;
                }
            } else if (h_mean < 0.3f) {
                for (int i = 0; i < result.stg_count; i++) {
                    const auto* node = stg_.get_node(result.stg_indices[i]);
                    if (node && node->tag < -0.3f) result.stg_scores[i] *= 1.2f;
                }
            }
        }
        return result;
    }

    RecallResult free_associate(const float* seed, float chaos_level) {
        RecallResult result;
        memset(&result, 0, sizeof(result));
        float temp = std::min(1.5f, chaos_level);
        float query[MEM_VEC_DIM];
        if (seed) {
            memcpy(query, seed, sizeof(float) * MEM_VEC_DIM);
        } else {
            for (int i = 0; i < MEM_VEC_DIM; i++)
                query[i] = (float)(rand() % 2000 - 1000) / 1000.0f;
        }
        float ltl_out[MEM_VEC_DIM];
        result.ltl_attractor_idx = ltl_.sample(query, temp,
            ltl_out, result.ltl_emotion, &result.ltl_score);
        if (result.ltl_attractor_idx >= 0)
            memcpy(result.ltl_vec, ltl_out, sizeof(float) * MEM_VEC_DIM);
        return result;
    }

    void update_temperatures(const float* hormone) {
        stg_.update_temperatures(hormone);
        ltl_.forget(avg_hormone(hormone));
    }

    void flashbulb_memory(int stg_idx, const float* emotion) {
        const auto* node = stg_.get_node(stg_idx);
        if (!node) return;
        for (int a = 0; a < ltl_.attractor_count(); a++) {
            const auto* attr = ltl_.get_attractor(a);
            if (!attr || !attr->active) continue;
            for (int s = 0; s < attr->stg_node_count; s++) {
                if (attr->stg_nodes[s] == stg_idx) {
                    ltl_.flashbulb_consolidate(a);
                    ltl_.set_emotion_label(a, emotion);
                    break;
                }
            }
        }
    }

    int stg_node_count() const { return stg_.node_count(); }
    int stg_edge_count() const { return stg_.edge_count(); }
    int ltl_attractor_count() const { return ltl_.attractor_count(); }
    void set_tick(uint64_t tick) { tick_ = tick; ltl_.set_tick(tick); }

    void debug_print() const {
        printf("[MEM] STG: %d nodes, %d edges | LTL: %d attractors | Tck=%llu\n",
               stg_.node_count(), stg_.edge_count(), ltl_.attractor_count(), tick_);
    }

private:
    STG stg_;
    LTL ltl_;
    uint64_t tick_;
    int tick_since_consolidation_;
    int tick_since_decay_;

    void consolidate_to_ltl(const float* hormone) {
        int count = stg_.node_count();
        if (count == 0) return;
        float vectors[STG_MAX_NODES * MEM_VEC_DIM];
        int indices[STG_MAX_NODES];
        int actual = 0;
        for (int i = 0; i < count && actual < STG_MAX_NODES; i++) {
            const auto* node = stg_.get_node(i);
            if (node && node->active && node->salience > 0.3f) {
                memcpy(vectors + actual * MEM_VEC_DIM, node->vec, sizeof(float) * MEM_VEC_DIM);
                indices[actual] = i;
                actual++;
            }
        }
        if (actual > 0) {
            ltl_.consolidate(vectors, indices, actual, hormone, tick_);
        }
    }

    void decay() { stg_.decay(1.0f); }

    static float avg_hormone(const float* h) {
        float s = 0;
        for (int i = 0; i < 16; i++) s += h[i];
        return s / 16.0f;
    }
};

} // namespace memory
} // namespace eos




