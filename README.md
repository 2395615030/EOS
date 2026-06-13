# EmotionOS — 完整项目蓝图与目录索引

## 项目愿景
从物理层到认知层，构建一个能通过连续感知流自发涌现"自我"的类脑系统。

## 目录结构
| 路径 | 内容 |
|------|------|
| docs/ | 设计文档、架构图、蓝图原文 |
| kernel/ | EmotionOS 软操作系统核心 |
| 
efs/ | NEFS 文件系统（DST一等公民、三阶内存映射） |
| 
n/ | 神经网络：情感核 + 记忆核 + 脑功能分层模型 |
| lgorithms/ | HABP算法、Triton算子链 |
| hardware/ | PDMS柔性皮肤、粘菌湿件系统、NFC边缘节点硬件设计 |
| utils/ | 构建工具、数据集、调试工具 |
| prototypes/ | 最小可行原型（MVP） |

## 层面索引

### 1. 物理计算层 -> hardware/
- PDMS柔性皮肤设计
- 粘菌湿件循环系统
- NFC PNN边缘节点

### 2. 软操作系统 -> kernel/
- EmotionOS软内核
- 协程调度（Windows Fiber）
- 软中断（激素触发）
- 三权限模型

### 3. 存储与内存 -> nefs/
- NEFS文件系统设计
- DST动态稀疏张量格式
- 三阶内存映射（M10->DDR->MX450）

### 4. 神经网络 -> nn/
- 情感神经网络（Affective Core）
- 记忆神经网络（Mnemonic Organ）
- 脑功能分层建模

### 5. 算法创新 -> algorithms/
- HABP赫布增强反向传播
- Triton自定义算子链
- 融合Transformer兼容设计

### 6. 认知架构 -> docs/ncf_architecture.md
- NCF事件驱动无主控架构
- 意识流循环设计

## 路线图
Phase 1: 情感核 + 记忆核 MVP（C++20）
Phase 2: Triton算子链 + CUDA后端
Phase 3: NEFS + 三阶内存映射
Phase 4: EmotionOS软内核
Phase 5: PDMS/粘菌硬件对接
Phase 6: NCF认知架构完整闭环
