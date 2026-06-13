# 自定义算法与算子链

## 1. HABP (Hebb-Augmented Backpropagation)
赫布学习增强反向传播算法

核心思想：
Delta_W = -eta * [BP梯度 - lambda * Hebb项]

其中：
- BP梯度 = 标准误差反向传播
- Hebb项 = y * x^T (局部活动依赖突触可塑性)
- lambda控制赫布影响强度（随训练衰减）

归一化：使用Oja规则抑制权重无限增长

## 2. Triton自定义算子链

### 标准算子（重写Transformer）
| 算子 | Triton Kernel | 说明 |
|------|---------------|------|
| GEMM | matmul_kernel | Block-level tile计算 |
| Softmax | softmax_kernel | 行级SRAM融合 |
| LayerNorm | layernorm_kernel | 融合均值和方差 |
| GELU | gelu_kernel | 融合激活 |

### 情感网络专用算子
| 算子 | 说明 |
|------|------|
| EmotionGate | 情绪门控：gate = sigmoid(w*e+b) |
| HormoneUpdate | 激素混沌更新 |
| EmotionFusion | 情绪-隐藏状态融合 |

### 记忆网络专用算子
| 算子 | 说明 |
|------|------|
| MemoryWrite | STG写入，Hebbian规则 |
| MemoryRead | LTL吸引子采样 |
| MemoryDecay | 情感温度驱动的遗忘 |

## 3. 融合策略
- 每个算子拆为三块：Triton kernel + autograd.Function + torch.nn.Module
- 标准Transformer算子和自定义算子通过同一计算图混合使用
- 支持与HuggingFace Transformers兼容
