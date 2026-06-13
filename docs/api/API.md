# EmotionOS API 参考

## 任务管理

| API | 说明 |
|-----|------|
| `e_sys_task_create(name, prio, entry, arg, stack)` | 创建任务 |
| `e_sys_task_exit(code)` | 退出当前任务 |
| `e_sys_task_sleep(ms)` | 睡眠指定毫秒 |
| `e_sys_task_yield()` | 主动让出CPU |
| `e_sys_task_self()` | 获取当前任务ID |
| `e_sys_task_getinfo(id, info)` | 查询任务信息 |
| `e_sys_task_kill(id)` | 终止任务 |
| `e_sys_task_set_priority(id, prio)` | 设置任务优先级 |
| `e_sys_task_list(out, count, max)` | 枚举任务 |
| `e_sys_task_suspend(id)` | 挂起任务 |
| `e_sys_task_resume(id)` | 恢复任务 |

## IPC (进程间通信)

| API | 说明 |
|-----|------|
| `e_sys_ipc_bind(port)` | 绑定到端口 |
| `e_sys_ipc_send(target, msg)` | 发送消息 (0=广播) |
| `e_sys_ipc_recv(msg, timeout_ms)` | 阻塞接收 |
| `e_sys_ipc_poll(msg)` | 非阻塞接收 |

## 内存管理

| API | 说明 |
|-----|------|
| `e_sys_mem_alloc(size, flags)` | 分配虚拟内存 |
| `e_sys_mem_free(addr)` | 释放内存 |
| `e_sys_mem_pin(addr, size)` | 锁定到物理内存 |
| `e_sys_mem_unpin(addr, size)` | 解锁 |

## 情感核

| API | 说明 |
|-----|------|
| `e_sys_affect_get(h, hc, e, ec)` | 读取激素/情绪状态 |
| `e_sys_affect_inject(stimulus, count)` | 注入外部刺激 |

## 记忆系统

| API | 说明 |
|-----|------|
| `e_sys_memory_write(data, count, tag)` | 写入记忆 |
| `e_sys_memory_recall(output, max, temp)` | 召回记忆 |

## 服务注册表

| API | 说明 |
|-----|------|
| `e_sys_service_register(name, port, prio, desc)` | 注册服务 |
| `e_sys_service_lookup(name, out)` | 查找服务 |
| `e_sys_service_list(out, count, max)` | 列出服务 |
| `e_sys_service_unregister(name)` | 取消注册 |

## 系统日志

| API | 说明 |
|-----|------|
| `e_sys_log(text)` | 写入日志 |
| `e_sys_logf(fmt, ...)` | 格式化写入 |
| `e_sys_log_read(out, count, max)` | 读取日志 |

## 系统

| API | 说明 |
|-----|------|
| `e_sys_shutdown()` | 关闭系统 |
| `e_sys_get_info()` | 获取系统信息 |
| `e_sys_program_load(path, name, prio)` | 加载程序 |
