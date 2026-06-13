# NEFS: Neural-Emotion File System

## 核心理念
把"动态稀疏张量（DST）"当作一等公民，而不是文件块。

## 设计目标
- DST为一等公民：支持张量热扩张/收缩
- 稀疏块级崩溃一致性
- 毫秒级快照
- 三阶内存映射原生支持

## 架构

### 超级块 (SuperBlock)
- 文件系统元数据：魔数、版本号、ChunkTable偏移、inode数量
- 64字节头部

### ChunkTable
- 索引DST稀疏块的物理位置
- 支持快速查找和热新增

### 稀疏块池
- 4MB最小粒度
- 块头包含：版本号、校验和、情感标签

### SAL日志 (Storage Attached Log)
- 崩溃一致性保证
- 写前日志 + 提交点

## DST格式
struct DST {
    vector<int64_t> shape;   // 任意阶，运行时决定
    struct Entry {
        vector<int64_t> idx;  // 坐标
        float value;           // 值
        float emotion_tag;     // 情感标签（关联情感网络）
    };
    vector<Entry> entries;    // 稀疏存储
    vector<bool> pinned;      // L1锁定标记
};

## 三阶内存映射集成
- L0: mmap + O_DIRECT (M10 NVMe) - 全量DST冷仓库
- L1: mlock 活跃稀疏块 (DDR4) - 热形状池
- L2: cudaMemcpyAsync 当前layer (MX450) - 计算片

## 构建顺序
1. DST数据结构 + 序列化（C++20标准库）
2. NEFS超级块 + ChunkTable
3. SAL日志 + 崩溃恢复
4. 三阶映射API：wpmmap_open / wpmmap_pin / wpmmap_unpin
5. 完整文件系统实现
