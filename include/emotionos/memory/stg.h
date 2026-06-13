// ============================================================
// EmotionOS - STG: Short-Term Plastic Graph
// ????? - SparseHopfield?? + Hebbian??
// ============================================================
#pragma once
#include "../types.h"
#include <vector>
#include <cmath>
#include <algorithm>
#include <cstdint>
#include <cstring>

namespace eos {
namespace memory {

// ---------- STG?? ----------
constexpr int STG_MAX_NODES   = 2048;   // ?????
constexpr int STG_VEC_DIM     = 128;    // ??????
constexpr int STG_TOP_K       = 8;      // ????top-k
constexpr int STG_WALK_STEPS  = 4;      // ??????

// ---------- ?? ----------
struct STGNode {
    float vec[STG_VEC_DIM];       // ????
    float hormone[16];            // ????????
    float emotion[8];             // ????????
    float tag;                    // ???? (0=neutral, 1=danger, -1=reward)
    float salience;               // ??? (0~1????????)
    uint64_t birth_tick;          // ???????tick
    uint64_t last_access;         // ??????
    float temperature;            // ???? (?=????=??)
    bool active;                  // ????
    
    STGNode() { clear(); }
    void clear() {
        memset(vec, 0, sizeof(vec));
        memset(hormone, 0, sizeof(hormone));
        memset(emotion, 0, sizeof(emotion));
        tag = 0.0f; salience = 0.5f;
        birth_tick = 0; last_access = 0;
        temperature = 0.5f; active = false;
    }
};

// ---------- ? (????) ----------
struct STGEdge {
    uint16_t from, to;    // ????
    float weight;         // ???? (Hebbian)
    float decay;          // ???
    
    STGEdge() : from(0), to(0), weight(0.0f), decay(0.01f) {}
    STGEdge(uint16_t f, uint16_t t, float w) 
        : from(f), to(t), weight(w), decay(0.01f) {}
};

// ---------- STG?? ----------
class STG {
public:
    STG() : node_count_(0), edge_count_(0), tick_(0) {}
    
    // ---- ?? ----
    // ???????? + ????????????
    int write(const float* vec, const float* hormone, const float* emotion,
              float tag, uint64_t tick) {
        tick_ = tick;
        
        // 1. ???????? (??????)
        int nearest = -1;
        float min_dist = 2.0f; // ??????
        for (int i = 0; i < node_count_; i++) {
            if (!nodes_[i].active) continue;
            float d = cosine_dist(vec, nodes_[i].vec);
            if (d < min_dist) { min_dist = d; nearest = i; }
        }
        
        // 2. ????? -> ??(??+??); ????
        const float MERGE_THRESHOLD = 0.15f; // ??????
        
        if (nearest >= 0 && min_dist < MERGE_THRESHOLD) {
            // ?????????????
            auto& n = nodes_[nearest];
            n.salience = std::min(1.0f, n.salience + 0.1f);
            n.last_access = tick;
            // ??????
            n.tag = n.tag * 0.7f + tag * 0.3f;
            // ????/?????
            for (int j = 0; j < 16; j++) 
                n.hormone[j] = n.hormone[j] * 0.8f + hormone[j] * 0.2f;
            for (int j = 0; j < 8; j++)
                n.emotion[j] = n.emotion[j] * 0.8f + emotion[j] * 0.2f;
            
            // Hebbian?????????
            hebbian_boost(nearest, vec);
            
            return nearest;
        } else {
            // ????
            if (node_count_ >= STG_MAX_NODES) {
                // ????????
                evict_least_salient();
            }
            int idx = node_count_++;
            auto& n = nodes_[idx];
            n.active = true;
            memcpy(n.vec, vec, sizeof(float) * STG_VEC_DIM);
            memcpy(n.hormone, hormone, sizeof(float) * 16);
            memcpy(n.emotion, emotion, sizeof(float) * 8);
            n.tag = tag;
            n.salience = 0.8f;
            n.birth_tick = tick;
            n.last_access = tick;
            n.temperature = 0.5f;
            
            // ??????????????
            if (nearest >= 0) {
                add_edge(idx, nearest, 0.5f);
                add_edge(nearest, idx, 0.5f);
            }
            
            return idx;
        }
    }
    
    // ---- ???? ----
    // ?????? + ??????? top-k ??????
    int recall(const float* query, const float* hormone, 
               int* out_indices, float* out_scores, int max_k) {
        int k = std::min(max_k, STG_TOP_K);
        if (node_count_ == 0) return 0;
        
        // 1. ????????????
        int seed = -1;
        float seed_sim = -2.0f;
        for (int i = 0; i < node_count_; i++) {
            if (!nodes_[i].active) continue;
            float s = cosine_sim(query, nodes_[i].vec);
            if (s > seed_sim) { seed_sim = s; seed = i; }
        }
        if (seed < 0) return 0;
        
        // 2. ??????????
        float visit_score[STG_MAX_NODES] = {0};
        visit_score[seed] = 1.0f;
        
        int current = seed;
        for (int step = 0; step < STG_WALK_STEPS; step++) {
            // ???????????
            int neighbors[64];
            float nweights[64];
            int ncount = 0;
            for (int e = 0; e < edge_count_ && ncount < 64; e++) {
                if (edges_[e].from == current && edges_[e].weight > 0.01f) {
                    neighbors[ncount] = edges_[e].to;
                    nweights[ncount] = edges_[e].weight;
                    ncount++;
                }
            }
            if (ncount == 0) break;
            
            // ??????????????
            float total_w = 0;
            for (int i = 0; i < ncount; i++) total_w += nweights[i];
            if (total_w < 0.001f) break;
            
            // ?????? (???????)
            float temp_factor = hormone ? 1.0f - avg_hormone(hormone) * 0.5f : 0.8f;
            for (int i = 0; i < ncount; i++) {
                int ni = neighbors[i];
                float decay_factor = nodes_[ni].temperature * 0.5f + 0.5f;
                visit_score[ni] += (nweights[i] / total_w) * temp_factor * decay_factor;
            }
            
            // ????
            float r = (float)rand() / RAND_MAX * total_w;
            float acc = 0;
            int next = current;
            for (int i = 0; i < ncount; i++) {
                acc += nweights[i];
                if (r <= acc) { next = neighbors[i]; break; }
            }
            current = next;
        }
        
        // 3. ? visit_score ??? top-k
        using Pair = std::pair<float, int>;
        std::vector<Pair> scored;
        for (int i = 0; i < node_count_; i++) {
            if (!nodes_[i].active) continue;
            float sim = cosine_sim(query, nodes_[i].vec);
            float final_score = visit_score[i] * 0.4f + sim * 0.6f;
            // ?????????????????
            final_score *= (1.5f - nodes_[i].temperature * 0.5f);
            scored.push_back({final_score, i});
        }
        std::sort(scored.begin(), scored.end(), 
                  [](const Pair& a, const Pair& b) { return a.first > b.first; });
        
        int found = std::min(k, (int)scored.size());
        for (int i = 0; i < found; i++) {
            out_indices[i] = scored[i].second;
            out_scores[i] = scored[i].first;
            // ??????
            nodes_[scored[i].second].last_access = tick_;
        }
        return found;
    }
    
    // ---- ?????? ----
    // ???????????
    void update_temperatures(const float* hormone) {
        float h_mean = 0;
        for (int i = 0; i < 16; i++) h_mean += hormone[i];
        h_mean /= 16.0f;
        
        for (int i = 0; i < node_count_; i++) {
            if (!nodes_[i].active) continue;
            // ??? ? ??? ? ???/??
            // ?????? ? ?? ? ?????
            float base_temp = 0.5f;
            float hormone_effect = h_mean * 0.3f;
            float salience_effect = (1.0f - nodes_[i].salience) * 0.3f;
            float age_effect = std::min(1.0f, (tick_ - nodes_[i].birth_tick) / 10000.0f) * 0.2f;
            
            nodes_[i].temperature = std::max(0.05f, std::min(0.95f, 
                base_temp + hormone_effect + salience_effect + age_effect));
        }
    }
    
    // ---- ?? ----
    // ????????????
    void decay(float dt) {
        // ???
        for (int i = 0; i < edge_count_; i++) {
            edges_[i].weight *= (1.0f - edges_[i].decay * dt);
            if (edges_[i].weight < 0.01f) {
                // ???? (????????)
                edges_[i] = edges_[edge_count_ - 1];
                edge_count_--;
                i--;
            }
        }
        // ????? (??????????)
        for (int i = 0; i < node_count_; i++) {
            if (!nodes_[i].active) continue;
            uint64_t age = tick_ - nodes_[i].last_access;
            if (age > 100) { // ??100tick???
                float decay_factor = 1.0f - std::min(1.0f, (age - 100) / 1000.0f) * 0.1f;
                nodes_[i].salience *= decay_factor;
                nodes_[i].temperature += 0.001f; // ?? -> ???
            }
        }
    }
    
    // ---- ???? ----
    const STGNode* get_node(int idx) const {
        if (idx < 0 || idx >= node_count_ || !nodes_[idx].active) return nullptr;
        return &nodes_[idx];
    }
    
    int node_count() const { return node_count_; }
    int edge_count() const { return edge_count_; }
    
    // ---- ?????????????????? ----
    float avg_tag() const {
        if (node_count_ == 0) return 0;
        float sum = 0; int c = 0;
        for (int i = 0; i < node_count_; i++) {
            if (nodes_[i].active) { sum += nodes_[i].tag; c++; }
        }
        return c > 0 ? sum / c : 0;
    }

private:
    STGNode nodes_[STG_MAX_NODES];
    STGEdge edges_[STG_MAX_NODES * 4]; // ?????????4??
    int node_count_;
    int edge_count_;
    uint64_t tick_;
    
    // ????? (-1~1)
    static float cosine_sim(const float* a, const float* b) {
        float dot = 0, na = 0, nb = 0;
        for (int i = 0; i < STG_VEC_DIM; i++) {
            dot += a[i] * b[i];
            na += a[i] * a[i];
            nb += b[i] * b[i];
        }
        float norm = sqrtf(na * nb);
        return norm > 1e-8f ? dot / norm : 0;
    }
    
    // ???? (0~2)
    static float cosine_dist(const float* a, const float* b) {
        return 1.0f - cosine_sim(a, b);
    }
    
    // ??????
    static float avg_hormone(const float* h) {
        float s = 0;
        for (int i = 0; i < 16; i++) s += h[i];
        return s / 16.0f;
    }
    
    // Hebbian??????????????????????
    void hebbian_boost(int node_idx, const float* input_vec) {
        for (int e = 0; e < edge_count_; e++) {
            if (edges_[e].from == node_idx) {
                int ni = edges_[e].to;
                float sim = cosine_sim(input_vec, nodes_[ni].vec);
                if (sim > 0.3f) {
                    // fire together, wire together
                    edges_[e].weight = std::min(1.0f, edges_[e].weight + sim * 0.05f);
                }
            }
        }
    }
    
    void add_edge(uint16_t from, uint16_t to, float weight) {
        if (edge_count_ >= STG_MAX_NODES * 4) return;
        // ???????
        for (int i = 0; i < edge_count_; i++) {
            if (edges_[i].from == from && edges_[i].to == to) {
                edges_[i].weight = std::min(1.0f, edges_[i].weight + weight * 0.1f);
                return;
            }
        }
        edges_[edge_count_++] = STGEdge(from, to, weight);
    }
    
    void evict_least_salient() {
        int worst = -1;
        float min_s = 1.0f;
        for (int i = 0; i < node_count_; i++) {
            if (!nodes_[i].active) continue;
            if (nodes_[i].salience < min_s) {
                min_s = nodes_[i].salience;
                worst = i;
            }
        }
        if (worst >= 0) {
            // ?????
            for (int i = 0; i < edge_count_; i++) {
                if (edges_[i].from == worst || edges_[i].to == worst) {
                    edges_[i] = edges_[--edge_count_];
                    i--;
                }
            }
            nodes_[worst].clear();
        }
    }
};

} // namespace memory
} // namespace eos

