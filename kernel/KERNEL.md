# EmotionOS 软内核设计

## 架构原则
- Windows用户态"管理员级"软操作系统
- 不写ASM、不写GRUB、不碰驱动层
- 靠VirtualAlloc / NtMapViewOfSection模拟内核能力

## 核心子系统

### 1. 软内核调度
- 基于Windows Fiber的单进程多协程调度
- 协程优先级映射三层权限模型
- 激素阈值可触发协程抢占（软中断）

### 2. 软页表
- VirtualAlloc管理虚拟地址空间
- NtMapViewOfSection实现共享内存
- 三阶内存映射：L0 NVMe -> L1 DDR -> L2 VRAM

### 3. 软中断
- 激素浓度超阈值 -> 触发协程切换
- 传感器数据就绪 -> 事件驱动唤醒
- 定时心跳 -> 周期性的激素代谢更新

### 4. 用户态NVMe驱动
- 轮询队列0/1
- 裸读NVMe BAR（通过MmMapIoSpace或等效API）
- 直接与NEFS文件系统对接

## 构建顺序
1. 协程调度器 + 软页表（基础运行时）
2. 软中断系统 + 激素触发机制
3. NVMe驱动 + 内存映射层
4. 完整EmotionOS引导流程

## 目录结构
- src/scheduler.cpp - 协程调度器
- src/pagetable.cpp - 软页表
- src/interrupt.cpp - 软中断
- src/nvme.cpp - 用户态NVMe驱动
- include/ - 头文件
- build.bat - 一键编译脚本
