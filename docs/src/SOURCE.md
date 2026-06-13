# EmotionOS 源码结构

## 目录布局

```
EmotionOS/
├── include/           # 公共头文件
│   ├── emotionos.h
│   ├── scheduler.h
│   ├── affect_service.h
│   └── emotionos/
│       ├── types.h          # 基础类型定义
│       ├── syscall.h        # 系统调用接口
│       ├── affect/          # 情感核子系统
│       ├── memory/          # 记忆子系统 (STG/LTL)
│       ├── nefs/            # NEFS文件系统
│       ├── security/        # 权限模型
│       └── shell/           # Shell引擎
├── src/               # 内核核心源码
│   ├── main.cpp             # 系统入口 + 组件引导
│   ├── kernel.cpp           # 微内核 (Fiber调度器)
│   └── scheduler.cpp        # 软内核调度器 (备用)
├── module/            # 内核模块 (可加载)
│   └── ...
├── libs/              # 预编译库
│   └── x86_64/
├── tests/             # 测试套件
│   ├── test_suite.cpp
│   ├── quick_test.cpp
│   └── ...
├── user/              # 用户程序示例
│   ├── user_app.cpp
│   └── user_program.cpp
├── sysapi/            # 用户API库
│   └── userlib.cpp
├── tools/             # 构建/部署工具
├── scripts/           # 开发脚本
├── prototypes/        # 最小可行原型
├── hardware/          # 硬件设计 (NFC/PDMS)
├── docs/              # 文档
│   ├── src/                 # 源码文档
│   ├── api/                 # API文档
│   ├── module/              # 模块文档
│   └── zh_cn/               # 中文文档
├── build.cmd          # 主构建脚本 (Windows)
└── Makefile           # 跨平台构建 (Unix/WSL)
```

## 内核设计

EmotionOS 的微内核基于 Windows Fiber 实现协作式多任务调度：

- **调度器**: 64任务槽，优先级队列 (ROOT/HIGH/NORMAL/LOW/IDLE)
- **IPC**: 基于端口的消息传递，支持广播/定向发送
- **内存**: VirtualAlloc 封装的虚拟内存管理
- **中断**: 软中断，IPI消息封装

## 子系统

| 子系统 | 头文件 | 说明 |
|--------|--------|------|
| 情感核 | include/emotionos/affect/ | 16激素+8情绪HPA轴模型 |
| 记忆 | include/emotionos/memory/ | STG短期+LTL长期双层记忆 |
| NEFS | include/emotionos/nefs/ | 持久化文件系统+情绪标签 |
| Shell | include/emotionos/shell/ | 命令注册+解析引擎 |
| 权限 | include/emotionos/security/ | ROOT/LOGIC/USER三重模型 |
