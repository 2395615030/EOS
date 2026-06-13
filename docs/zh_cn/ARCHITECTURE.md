# EmotionOS 项目文档

## 项目架构

EmotionOS 是一个用户态"软操作系统"，从零构建的类脑系统，具有情感核、记忆器官和持久化文件系统。

### 内核层

基于 Windows Fiber 实现协作式多任务调度，提供完整的系统调用接口。

### 情感核

仿 HPA 轴的激素-情绪耦合模型，16维激素向量驱动8维情绪状态。

### 记忆器官

双层记忆结构：
- **STG** (Short-Term Plastic Graph): 短期可塑图，Hebbian学习
- **LTL** (Long-Term Learning): 长期吸引子网络，记忆巩固

### 文件系统

NEFS (Nested Emotion Flattening Store)：具有情感标签的持久化张量存储。

## Shell命令

| 命令 | 说明 |
|------|------|
| `service list` | 列出服务 |
| `ps` | 任务列表 |
| `ls / mkdir / rm / mv` | 文件操作 |
| `affect / horm / emotion` | 情感状态 |
| `log` | 系统日志 |
| `run <path>` | 加载程序 |
