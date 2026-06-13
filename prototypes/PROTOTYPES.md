# 原型开发指南

## Phase 1: 情感核 + 记忆核 MVP（C++20）
目标：输入128-d向量S，输出16-d激素H和8-d情绪E，且H会自循环
时间：约2周
文件：prototypes/affect_memory_mvp/

核心文件：
- main.cpp - 入口，1000步循环
- affect.cpp/h - 情感核混沌更新
- memory.cpp/h - 记忆核STG+LTL
- rng.h - 混沌随机数发生器

验证方式：printf实时输出激素/情绪曲线

## Phase 2: Triton算子链
目标：用Triton实现GEMM/Softmax/LayerNorm + 情绪门控/记忆读写算子
时间：约3周
文件：prototypes/triton_ops/

## Phase 3: NEFS原型
目标：DST结构序列化/反序列化，mmap映射
时间：约2周
文件：prototypes/nefs_proto/

## 当前推荐
从Phase 1开始，先让"情感-记忆闭环"跑起来，给你一点"活的"感觉。
