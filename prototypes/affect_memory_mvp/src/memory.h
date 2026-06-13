#pragma once
#include "types.h"
#include <unordered_map>
#include <vector>
#include <algorithm>
#include <cmath>
#include <cstdio>

// ============================================================
// 记忆神经网络 (Mnemonic Organ)
// 三层存储：感知缓存 -> STG短期可塑图 -> LTL长期可塑地形
// ============================================================

// 微概念节点
struct ConceptNode {
    StimulusVec feature{};   // 特征向量
    float salience = 0.0f;   // 显著性
    float emotion_tag = 0.0f; // 情感标签
    int birth_time = 0;      // 创建时间
    int access_count = 0;    // 访问计数
};

// 短期可塑图 (STG)
class STG {
public:
    static constexpr int MAX_NODES = 512;
    static constexpr float HEBBIAN_LR = 0.01f;
    static constexpr float FORGET_RATE = 0.001f;
    
    STG() = default;
    
    // 写入事件
    int write(const StimulusVec& feature, const HormoneVec& H, const EmotionVec& E, int time) {
        // 计算情感标签（从激素和情绪中提取标量）
        float emotion_tag = 0.0f;
        for (auto& h : H) emotion_tag += h;
        for (auto& e : E) emotion_tag += std::abs(e);
        emotion_tag /= (H.size() + E.size());
        
        // 查找是否已有相似节点
        for (size_t i = 0; i < nodes_.size(); i++) {
            float sim = cosine_sim(nodes_[i].feature, feature);
            if (sim > 0.85f) {
                // 强化已有节点
                nodes_[i].salience = std::min(1.0f, nodes_[i].salience + 0.1f);
                nodes_[i].access_count++;
                nodes_[i].emotion_tag = emotion_tag * 0.3f + nodes_[i].emotion_tag * 0.7f;
                
                // Hebbian: 增加与最近活跃节点的连接
                if (!active_path_.empty()) {
                    int last = active_path_.back();
                    if (last != (int)i) {
                        edges_[{last, (int)i}] += HEBBIAN_LR;
                        edges_[{(int)i, last}] += HEBBIAN_LR;
                    }
                }
                active_path_.push_back((int)i);
                return (int)i;
            }
        }
        
        // 创建新节点
        if (nodes_.size() >= MAX_NODES) {
            // 淘汰最不显著的节点
            evict_least_salient();
        }
        
        ConceptNode node;
        node.feature = feature;
        node.salience = 0.5f;
        node.emotion_tag = emotion_tag;
        node.birth_time = time;
        node.access_count = 1;
        
        int idx = (int)nodes_.size();
        nodes_.push_back(node);
        
        // 连接到最近活跃节点
        if (!active_path_.empty()) {
            int last = active_path_.back();
            edges_[{last, idx}] = 0.3f;
            edges_[{idx, last}] = 0.3f;
        }
        active_path_.push_back(idx);
        
        return idx;
    }
    
    // 联想：从查询向量出发，随机游走
    std::vector<int> associate(const StimulusVec& query, int steps = 3) {
        // 找最近节点
        int start = -1;
        float best_sim = -1.0f;
        for (size_t i = 0; i < nodes_.size(); i++) {
            float sim = cosine_sim(nodes_[i].feature, query);
            if (sim > best_sim) {
                best_sim = sim;
                start = (int)i;
            }
        }
        
        if (start < 0) return {};
        
        // 随机游走
        std::vector<int> path;
        path.push_back(start);
        
        int current = start;
        ChaosRng rng;
        
        for (int s = 0; s < steps; s++) {
            // 收集邻居
            std::vector<std::pair<int, float>> neighbors;
            for (auto& [key, weight] : edges_) {
                if (key.first == current && weight > 0.1f) {
                    neighbors.push_back({key.second, weight});
                }
            }
            
            if (neighbors.empty()) break;
            
            // 按权重采样
            float total = 0.0f;
            for (auto& [_, w] : neighbors) total += w;
            
            float r = rng.noise() * 0.5f + 0.5f; // 映射到[0,1)
            r *= total;
            
            float accum = 0.0f;
            for (auto& [node, w] : neighbors) {
                accum += w;
                if (r <= accum) {
                    current = node;
                    break;
                }
            }
            
            path.push_back(current);
        }
        
        return path;
    }
    
    // 获取节点
    const ConceptNode& node(int idx) const { return nodes_[idx]; }
    size_t size() const { return nodes_.size(); }
    
    // 获取活跃子图权重（供情感核雕刻）
    void get_live_weights(std::array<std::array<float, 152>, 8>& W_out) {
        // MVP简化版：从最近路径中的节点提取特征影响
        if (active_path_.empty()) return;
        
        int last = active_path_.back();
        auto& feat = nodes_[last].feature;
        
        // 把节点的特征注入权重的前128维
        for (size_t i = 0; i < 8 && i < nodes_.size(); i++) {
            for (size_t j = 0; j < 128; j++) {
                W_out[i][j] += feat[j] * 0.01f;
            }
        }
    }
    
    // 衰减：所有边的权重随时间衰减
    void decay_edges() {
        std::vector<std::pair<std::pair<int,int>, float>> to_remove;
        for (auto& [key, weight] : edges_) {
            edges_[key] = weight * (1.0f - FORGET_RATE);
            if (edges_[key] < 0.01f) {
                to_remove.push_back({key, edges_[key]});
            }
        }
        for (auto& [key, _] : to_remove) {
            edges_.erase(key);
        }
    }
    
    // 打印状态
    void print_summary() const {
        std::printf("    STG: nodes=%zu edges=%zu active_path=%zu\n",
                    nodes_.size(), edges_.size(), active_path_.size());
    }
    
private:
    std::vector<ConceptNode> nodes_;
    std::unordered_map<std::pair<int,int>, float, 
        decltype([](const std::pair<int,int>& p) { 
            return std::hash<int>()(p.first) ^ (std::hash<int>()(p.second) << 1); 
        })> edges_;
    std::vector<int> active_path_;
    
    float cosine_sim(const StimulusVec& a, const StimulusVec& b) {
        float dot = 0.0f, na = 0.0f, nb = 0.0f;
        for (size_t i = 0; i < a.size(); i++) {
            dot += a[i] * b[i];
            na += a[i] * a[i];
            nb += b[i] * b[i];
        }
        float denom = std::sqrt(na) * std::sqrt(nb);
        return denom > 1e-8f ? dot / denom : 0.0f;
    }
    
    void evict_least_salient() {
        auto it = std::min_element(nodes_.begin(), nodes_.end(),
            [](const ConceptNode& a, const ConceptNode& b) {
                return a.salience < b.salience;
            });
        if (it != nodes_.end()) {
            int idx = (int)(it - nodes_.begin());
            // 清除相关边
            std::vector<std::pair<int,int>> to_remove;
            for (auto& [key, _] : edges_) {
                if (key.first == idx || key.second == idx) {
                    to_remove.push_back(key);
                }
            }
            for (auto& key : to_remove) edges_.erase(key);
            nodes_.erase(it);
        }
    }
};

// 长期可塑地形 (LTL) - MVP简化版
class LTL {
public:
    static constexpr int GRID_SIZE = 64;
    
    LTL() {
        for (auto& patch : landscape_) {
            for (auto& v : patch) v = 0.0f;
        }
    }
    
    // 把STG子图压印到长期地形
    void compress(const STG& stg) {
        if (stg.size() == 0) return;
        
        float influence = 1.0f / (float)GRID_SIZE;
        
        for (size_t i = 0; i < stg.size() && i < GRID_SIZE * GRID_SIZE; i++) {
            int idx = (int)i;
            auto& node = stg.node(i);
            
            int x = (idx * 7) % GRID_SIZE;
            int y = (idx * 13) % GRID_SIZE;
            
            // 用节点显著性影响地形
            landscape_[x][y] += node.salience * influence * 0.1f;
            // 衰减避免无限增长
            if (landscape_[x][y] > 1.0f) landscape_[x][y] = 1.0f;
        }
    }
    
    // 从地形采样联想结果（简化版：返回热点区域特征）
    StimulusVec sample(const StimulusVec& query, float temperature) {
        // 找最热点
        int best_x = 0, best_y = 0;
        float best_val = -1.0f;
        for (int x = 0; x < GRID_SIZE; x++) {
            for (int y = 0; y < GRID_SIZE; y++) {
                if (landscape_[x][y] > best_val) {
                    best_val = landscape_[x][y];
                    best_x = x;
                    best_y = y;
                }
            }
        }
        
        // 根据热度和温度，决定是否返回刺激本身还是"联想"结果
        // 温度高 = 更有创意/不稳定的联想
        StimulusVec result = query;
        float influence = best_val * temperature;
        
        ChaosRng rng;
        for (auto& v : result) {
            v += rng.noise() * influence * 0.1f;
        }
        
        return result;
    }
    
    // 地形全局衰减
    void global_decay(float rate = 0.999f) {
        for (auto& row : landscape_)
            for (auto& v : row)
                v *= rate;
    }
    
    // 打印地形统计
    void print_summary() const {
        float mean = 0.0f, max_val = 0.0f;
        int hot_count = 0;
        for (auto& row : landscape_) {
            for (auto& v : row) {
                mean += v;
                if (v > max_val) max_val = v;
                if (v > 0.5f) hot_count++;
            }
        }
        mean /= (GRID_SIZE * GRID_SIZE);
        std::printf("    LTL: mean=%.4f max=%.4f hot_spots=%d\n",
                    mean, max_val, hot_count);
    }
    
private:
    std::array<std::array<float, GRID_SIZE>, GRID_SIZE> landscape_{};
};

// 统一记忆网络接口
class MemoryCore {
public:
    MemoryCore() = default;
    
    // 写入记忆
    void write(const StimulusVec& S, const HormoneVec& H, const EmotionVec& E, int time) {
        stg_.write(S, H, E, time);
        
        // 每10步压缩到LTL
        if (time % 10 == 0) {
            ltl_.compress(stg_);
        }
        
        // 每步边缘衰减
        stg_.decay_edges();
        ltl_.global_decay();
    }
    
    // 自由联想
    StimulusVec recall(const StimulusVec& query, float temperature) {
        // 1. STG联想路径
        auto path = stg_.associate(query);
        
        // 2. LTL采样
        StimulusVec result = ltl_.sample(query, temperature);
        
        // 如果STG有活跃路径，用路径中的节点特征调制结果
        if (!path.empty()) {
            auto& last_node = stg_.node(path.back());
            for (size_t i = 0; i < result.size(); i++) {
                result[i] = result[i] * 0.7f + last_node.feature[i] * 0.3f;
            }
        }
        
        return result;
    }
    
    // 获取供情感网络雕刻的权重
    void get_live_weights(std::array<std::array<float, 152>, 8>& W_out) {
        stg_.get_live_weights(W_out);
    }
    
    // 暴露STG引用供main打印
    const STG& stg() const { return stg_; }
    
    // 打印状态
    void print_summary() const {
        stg_.print_summary();
        ltl_.print_summary();
    }
    
private:
    STG stg_;
    LTL ltl_;
};
