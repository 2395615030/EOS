// EmotionOS - Complete Shell Commands v0.7
// All command groups covering every subsystem
#pragma once
#include "emotionos/types.h"
#include "emotionos/syscall.h"
#include "emotionos/shell/shell.h"
#include "emotionos/affect/affective_core.h"
#include "emotionos/memory/mnemonic.h"
#include "emotionos/nefs/nefs.h"
#include "emotionos/security/permissions.h"
#include <cstdio>
#include <cstring>
#include <cmath>
#include <cstdint>

// Forward declarations from main.cpp
extern eos::affect::AffectiveCore g_affect;
extern eos::memory::MnemonicOrgan* g_mem;
extern int g_mem_writes;
extern eos::nefs::NEFS* g_nefs;

namespace eos { namespace shell {

static void print_affect_summary() {
    float h[16], e[8]; int hc = 16, ec = 8;
    e_sys_affect_get(h, &hc, e, &ec);
    const char* en[] = {"Joy","Sad","Ang","Fear","Surp","Disg","Trst","Antc"};
    int emax = 0; for(int i = 1; i < 8; i++) if(fabsf(e[i]) > fabsf(e[emax])) emax = i;
    printf("  H: DA=%.2f CO=%.2f SE=%.2f NA=%.2f AD=%.2f | E: %s %+.3f\n",
        h[0], h[1], h[2], h[3], h[6], en[emax], e[emax]);
}

static const char* task_state_str(etstate_t s) {
    switch(s) {
        case ETS_READY: return "RDY";
        case ETS_RUNNING: return "RUN";
        case ETS_BLOCKED: return "BLK";
        case ETS_SLEEPING: return "SLP";
        case ETS_TERMINATED: return "END";
        case ETS_WAITING: return "WAI";
        default: return "?";
    }
}

inline void register_all_commands() {
    auto& R = CmdRegistry::instance();

    // -------- HELP / SYSTEM --------
    R.register_cmd("help", "Show help or help [topic]", [](const CmdLine& c) {
        if (c.argc > 0) CmdRegistry::instance().print_help(c.str(0));
        else CmdRegistry::instance().print_help();
    });
    R.register_cmd("version", "Show EmotionOS version info", [](const CmdLine&) {
        printf("  EmotionOS v0.7 - Shell Complete\n");
        printf("  Kernel: microkernel v0.3 (Fiber scheduler)\n");
        printf("  NEFS: v1.0 (Nested Emotion Flattening Store)\n");
        printf("  Affect: AffectiveCore v0.6 (16H+8E)\n");
        printf("  Memory: MnemonicOrgan v0.1 (STG+STL)\n");
    });
    R.register_cmd("uptime", "System uptime", [](const CmdLine&) {
        auto i = e_sys_get_info();
        uint64_t ms = i.uptime_ms;
        uint64_t sec = ms / 1000;
        printf("  Uptime: %llu days, %02llu:%02llu:%02llu\n",
            sec / 86400, (sec % 86400) / 3600, (sec % 3600) / 60, sec % 60);
        printf("  Tick: %llu | Switches: %llu\n", i.tick, i.total_context_switches);
    });
    R.register_cmd("stats", "Full system statistics", [](const CmdLine&) {
        auto i = e_sys_get_info();
        printf("=== EmotionOS System Stats ===\n");
        printf("  Tick: %llu | Tasks: %d/%d\n", i.tick, i.active_task_count, i.task_count);
        printf("  Switches: %llu | Uptime: %llums\n", i.total_context_switches, i.uptime_ms);
        printf("  Hormone mean: %.3f max: %.3f[%d]\n", i.hormone_mean, i.hormone_max, i.hormone_max_idx);
        printf("  Memory writes: %d\n", g_mem_writes);
        if (g_mem) {
            printf("  STG nodes: %d | LTL attractors: %d\n",
                g_mem->stg_node_count(), g_mem->ltl_attractor_count());
        }
        if (g_nefs) {
            printf("  NEFS inodes: %d chunks: %d disk: %.1f%%\n",
                g_nefs->inode_count(), g_nefs->chunk_count(),
                g_nefs->disk_usage_pct());
        }
        print_affect_summary();
    });
    R.register_cmd("dmesg", "View system log events", [](const CmdLine& c) {
        printf("  System log not yet implemented - see monitor output.\n");
    });
    R.register_cmd("echo", "Echo arguments", [](const CmdLine& c) {
        printf("  "); for(int i=0;i<c.argc;i++) printf("%s ",c.str(i)); printf("\n");
    });
    R.register_cmd("clear", "Clear screen (newlines)", [](const CmdLine&) {
        for(int i=0;i<50;i++) printf("\n");
    });

    // -------- TASK MANAGEMENT --------
    R.register_cmd("ps", "List all tasks with state", [](const CmdLine&) {
        uint32_t count = 0;
        etask_t ids[64];
        e_sys_task_list(ids, &count, 64);
        printf("  %-4s %-16s %-6s %-8s %s\n", "ID", "NAME", "STATE", "PRIO", "TICKS");
        printf("  ---- ---------------- ------ -------- ------\n");
        for (uint32_t i = 0; i < count; i++) {
            etask_info_t ti;
            if (e_sys_task_getinfo(ids[i], &ti) == 0) {
                const char* pn = "?";
                switch (ti.priority) {
                    case 0: pn = "ROOT"; break;
                    case 1: pn = "HIGH"; break;
                    case 2: pn = "NORM"; break;
                    case 3: pn = "LOW"; break;
                    case 4: pn = "IDLE"; break;
                }
                printf("  [%d] %-16s %-6s %-8s %llu\n",
                    ti.id, ti.name ? ti.name : "?", task_state_str(ti.state), pn, ti.total_ticks);
            }
        }
    });
    R.register_cmd("kill", "Terminate a task: kill [id]", [](const CmdLine& c) {
        if (c.argc < 1) { printf("  Usage: kill [task_id]\n"); return; }
        etask_t id = (etask_t)c.integer(0);
        if (e_sys_task_kill(id) == 0) printf("  Task [%d] terminated.\n", id);
        else printf("  Failed to kill task [%d]\n", id);
    });
    R.register_cmd("priority", "Set task priority: priority [id] [0-4]", [](const CmdLine& c) {
        if (c.argc < 2) { printf("  Usage: priority [task_id] [0=ROOT 1=HIGH 2=NORM 3=LOW 4=IDLE]\n"); return; }
        etask_t id = (etask_t)c.integer(0);
        eprio_t prio = (eprio_t)(c.integer(1) & 3);
        if (e_sys_task_set_priority(id, prio) == 0)
            printf("  Task [%d] priority set to %d.\n", id, prio);
        else printf("  Failed.\n");
    });
    R.register_cmd("suspend", "Suspend a task: suspend [id]", [](const CmdLine& c) {
        if (c.argc < 1) { printf("  Usage: suspend [task_id]\n"); return; }
        etask_t id = (etask_t)c.integer(0);
        if (e_sys_task_suspend(id) == 0) printf("  Task [%d] suspended.\n", id);
        else printf("  Failed (task may already be terminated or invalid).\n");
    });
    R.register_cmd("resume", "Resume a task: resume [id]", [](const CmdLine& c) {
        if (c.argc < 1) { printf("  Usage: resume [task_id]\n"); return; }
        etask_t id = (etask_t)c.integer(0);
        if (e_sys_task_resume(id) == 0) printf("  Task [%d] resumed.\n", id);
        else printf("  Failed (task not suspended or invalid).\n");
    });

    // -------- FILESYSTEM (NEFS) --------
    R.register_cmd("ls", "List directory: ls [path]", [](const CmdLine& c) {
        if (!g_nefs) { printf("  NEFS not mounted\n"); return; }
        const char* path = c.str(0) ? c.str(0) : "/";
        uint32_t id = g_nefs->resolve(path);
        if (!id) { printf("  Path not found: %s\n", path); return; }
        printf("  %-36s %-6s %-8s %s\n", "NAME", "TYPE", "ENTRIES", "FLAGS");
        printf("  %-36s %-6s %-8s %s\n", "----", "----", "-------", "-----");
        for (uint32_t i = 0; i < g_nefs->inode_count(); i++) {
            auto* in = g_nefs->get_inode(i + 1);
            if (in && in->parent_id == id && in->id != id) {
                const char* t = in->type == 1 ? "[DIR]" : "[TEN]";
                printf("  %-36s %-6s %-8u %08x\n",
                    in->name, t, in->entry_count, in->flags);
            }
        }
    });
    R.register_cmd("mkdir", "Create directory: mkdir [path]", [](const CmdLine& c) {
        if (!g_nefs) { printf("  NEFS not mounted\n"); return; }
        if (c.argc < 1) { printf("  Usage: mkdir [path]\n"); return; }
        uint32_t id = g_nefs->mkdir(c.str(0));
        if (id) printf("  Created: %s (id=%u)\n", c.str(0), id);
        else printf("  Failed.\n");
    });
    R.register_cmd("rm", "Remove file/dir: rm [path]", [](const CmdLine& c) {
        if (!g_nefs) { printf("  NEFS not mounted\n"); return; }
        if (c.argc < 1) { printf("  Usage: rm [path]\n"); return; }
        if (g_nefs->remove(c.str(0))) printf("  Removed: %s\n", c.str(0));
        else printf("  Failed (not found or not empty).\n");
    });
    R.register_cmd("mv", "Move/rename: mv [src] [dst]", [](const CmdLine& c) {
        if (!g_nefs) { printf("  NEFS not mounted\n"); return; }
        if (c.argc < 2) { printf("  Usage: mv [src_path] [new_name]\n"); return; }
        if (g_nefs->rename(c.str(0), c.str(1))) printf("  Renamed.\n");
        else printf("  Failed.\n");
    });
    R.register_cmd("df", "NEFS disk usage", [](const CmdLine&) {
        if (!g_nefs) { printf("  NEFS not mounted\n"); return; }
        printf("  Capacity: %llu bytes\n", g_nefs->disk_capacity());
        printf("  Used:     %llu bytes (%.1f%%)\n", g_nefs->disk_used(), g_nefs->disk_usage_pct());
        printf("  Inodes:   %u / %d\n", g_nefs->inode_count(), eos::nefs::INODE_TABLE_SIZE);
        printf("  Chunks:   %u / %d\n", g_nefs->chunk_count(), eos::nefs::CHUNK_POOL_SIZE);
    });
    R.register_cmd("du", "Tensor size info: du [path]", [](const CmdLine& c) {
        if (!g_nefs) { printf("  NEFS not mounted\n"); return; }
        const char* path = c.str(0);
        if (path) {
            auto si = g_nefs->stat_info(path);
            if (si.id) printf("  %s: id=%u entries=%u chunks=%u\n",
                path, si.id, si.entry_count, si.chunk_count);
            else printf("  Not found.\n");
        } else {
            printf("  Total tensors: %u | Total entries: varies\n", g_nefs->inode_count());
        }
    });
    R.register_cmd("chmod", "Set inode flags: chmod [id] [flags]", [](const CmdLine& c) {
        if (!g_nefs) { printf("  NEFS not mounted\n"); return; }
        if (c.argc < 2) { printf("  Usage: chmod [inode_id] [hex_flags]\n"); return; }
        uint32_t id = (uint32_t)c.integer(0);
        uint32_t flags = (uint32_t)c.integer(1);
        if (g_nefs->chmod(id, flags)) printf("  Inode %u flags set to %08x.\n", id, flags);
        else printf("  Failed.\n");
    });
    R.register_cmd("mount", "Mount NEFS image: mount [path]", [](const CmdLine& c) {
        if (!g_nefs) { printf("  NEFS not initialized\n"); return; }
        const char* p = c.str(0) ? c.str(0) : "D:\\Project\\EmotionOS\\kernel\\nefs.img";
        g_nefs->mount(p); g_nefs->print_tree();
    });
    R.register_cmd("umount", "Umount NEFS (flush to disk)", [](const CmdLine&) {
        if (!g_nefs) { printf("  NEFS not initialized\n"); return; }
        g_nefs->umount(); printf("  Flushed.\n");
    });
    R.register_cmd("tree", "Print NEFS tree", [](const CmdLine&) {
        if (!g_nefs) { printf("  NEFS not mounted\n"); return; }
        g_nefs->print_tree();
    });

    // -------- AFFECT / EMOTION --------
    R.register_cmd("affect", "Affect core status", [](const CmdLine&) {
        g_affect.print();
    });
    R.register_cmd("horm", "Hormone details", [](const CmdLine& c) {
        float h[16], e[8]; int hc = 16, ec = 8;
        e_sys_affect_get(h, &hc, e, &ec);
        printf("  Hormones:\n");
        const char* hn[] = {"DA","CO","SE","NA","OXY","ENDO","ADR","MEL"};
        for (int i = 0; i < 8; i++) printf("    %s=%.3f ", hn[i], h[i]);
        printf("\n");
        printf("  Emotions: ");
        const char* en[] = {"Joy","Sad","Ang","Fear","Surp","Disg","Trst","Antc"};
        for (int i = 0; i < 8; i++) printf("%s%+.2f ", en[i], e[i]);
        printf("\n");
        printf("  Sculpting: %s\n", g_affect.is_sculpting_active() ? "active" : "static");
    });
    R.register_cmd("emotion", "Emotion watch/subcmds", [](const CmdLine& c) {
        if(c.argc < 1) { g_affect.print(); return; }
        const char* sub = c.str(0);
        if(strcmp(sub,"watch")==0 || strcmp(sub,"w")==0) {
            print_affect_summary();
        } else if(strcmp(sub,"sculpt")==0) {
            g_affect.enable_sculpting(c.argc < 2 || c.integer(1) != 0);
            printf("  Sculpting: %s\n", g_affect.is_sculpting_active() ? "ON" : "OFF");
        } else if(strcmp(sub,"inject")==0 && c.argc >= 2) {
            float v = c.flt(1);
            float stim[128] = {0};
            for(int i=0;i<16;i++) stim[i] = v;
            e_sys_affect_inject(stim, 128);
            printf("  Injected stimulus %.2f to affect core.\n", v);
        } else {
            printf("  Subcmds: watch, sculpt [0|1], inject [val]\n");
        }
    });
    R.register_cmd("noise", "Set affect noise gain: noise [0.0-2.0]", [](const CmdLine& c) {
        printf("  Noise gain control: not directly exposed (use affect sculpt)\n");
    });

    // -------- MEMORY --------
    R.register_cmd("mem", "Memory system status", [](const CmdLine&) {
        if (!g_mem) { printf("  Memory not initialized\n"); return; }
        printf("  === Memory Organ ===\n");
        printf("  STG nodes: %d | Edges: varies\n", g_mem->stg_node_count());
        printf("  LTL attractors: %d\n", g_mem->ltl_attractor_count());
        printf("  Total writes: %d\n", g_mem_writes);
    });
    R.register_cmd("recall", "Memory recall query", [](const CmdLine& c) {
        if (!g_mem) { printf("  Memory not initialized\n"); return; }
        printf("  Recall via IPC: not directly callable from shell.\n");
        printf("  Use 'mem' to see state.\n");
    });
    R.register_cmd("flashbulb", "Show flashbulb memory stats", [](const CmdLine&) {
        if (!g_mem) { printf("  Memory not initialized\n"); return; }
        printf("  Flashbulb events are internal - see 'mem' for LTL count.\n");
    });

    // -------- IPC --------
    R.register_cmd("ipc", "IPC subcommands: ipc [bind|send|stats]", [](const CmdLine& c) {
        if (c.argc < 1) { printf("  Usage: ipc bind [port] | ipc send [task] [msg] | ipc stats\n"); return; }
        const char* sub = c.str(0);
        if (strcmp(sub, "bind") == 0) {
            int port = c.argc > 1 ? (int)c.integer(1) : 0;
            if (e_sys_ipc_bind((eport_t)port) == 0) printf("  Bound to port %d.\n", port);
            else printf("  Failed to bind port %d.\n", port);
        } else if (strcmp(sub, "send") == 0 && c.argc >= 3) {
            etask_t t = (etask_t)c.integer(1);
            eipc_msg_t m; memset(&m, 0, sizeof(m));
            m.type = EIPC_USER;
            m.code = 0;
            m.fvalue = c.flt(2);
            if (e_sys_ipc_send(t, &m) == 0) printf("  Sent message to [%d].\n", t);
            else printf("  Failed.\n");
        } else if (strcmp(sub, "stats") == 0) {
            printf("  IPC system is fiber-based, no persistent stats yet.\n");
        } else {
            printf("  Unknown ipc subcommand: %s\n", sub);
        }
    });

    // -------- MEMORY MANAGEMENT --------
    R.register_cmd("alloc", "Allocate kernel memory: alloc [size]", [](const CmdLine& c) {
        if (c.argc < 1) { printf("  Usage: alloc [bytes]\n"); return; }
        size_t sz = (size_t)c.integer(0);
        if (sz > 1048576) { printf("  Too large (max 1MB).\n"); return; }
        void* p = e_sys_mem_alloc(sz, EMEM_RW);
        if (p) printf("  Allocated %zu bytes at %p.\n", sz, p);
        else printf("  Allocation failed.\n");
        e_sys_mem_free(p);
    });
    R.register_cmd("meminfo", "Memory usage info", [](const CmdLine&) {
        auto i = e_sys_get_info();
        printf("  System memory: VirtualAlloc-based (no stats tracked)\n");
        printf("  Tasks: %d active / %d total\n", i.active_task_count, i.task_count);
        if (g_mem) printf("  Memory organ: %d nodes\n", g_mem->stg_node_count());
    });

    
    // -------- SERVICE REGISTRY --------
    R.register_cmd("service", "Service subcommands: service [list|lookup|register]", [](const CmdLine& c) {
        if (c.argc < 1) { printf("  Usage: service list | service lookup [name] | service register [name] [port] [desc]\n"); return; }
        const char* sub = c.str(0);
        if (strcmp(sub, "list") == 0) {
            eservice_entry_t svcs[32]; uint32_t n = 0;
            if (e_sys_service_list(svcs, &n, 32) == 0) {
                printf("  %-24s %-6s %-6s %s\n", "NAME", "PORT", "PRIO", "STATUS");
                printf("  %-24s %-6s %-6s %s\n", "----", "----", "----", "------");
                for (uint32_t i = 0; i < n; i++)
                    printf("  %-24s %-6u %-6u %s\n", svcs[i].name, svcs[i].port, svcs[i].priority,
                        svcs[i].running ? "RUN" : "STOP");
            } else printf("  No services.\n");
        } else if (strcmp(sub, "lookup") == 0 && c.argc >= 2) {
            eservice_entry_t s;
            if (e_sys_service_lookup(c.str(1), &s) == 0)
                printf("  %s: port=%u prio=%u desc=%s running=%s\n",
                    s.name, s.port, s.priority, s.description, s.running ? "yes" : "no");
            else printf("  Service not found.\n");
        } else if (strcmp(sub, "register") == 0 && c.argc >= 3) {
            const char* desc = c.argc > 3 ? c.str(3) : "user registered";
            int port = (int)c.integer(1);
            if (e_sys_service_register(c.str(0), (eport_t)port, EPRIO_LOW, desc) == 0)
                printf("  Service '%s' registered on port %d.\n", c.str(0), port);
            else printf("  Failed (duplicate name or full).\n");
        } else {
            printf("  Unknown subcommand: %s\n", sub);
        }
    });

    R.register_cmd("run", "Run an external program: run [exe_path]", [](const CmdLine& c) {
        if (c.argc < 1) { printf("  Usage: run [exe_path]\n"); return; }
        const char* path = c.str(0);
        estatus_t r = e_sys_program_load(path, "user_prog", EPRIO_LOW);
        if (r == 0) printf("  Loaded: %s\n", path);
        else if (r == -1) printf("  Failed to load DLL: %s\n", path);
        else if (r == -2) printf("  DLL loaded but no 'e_program_main' export found.\n");
    });

    R.register_cmd("log", "View system log: log [count]", [](const CmdLine& c) {
        elog_entry_t entries[128]; uint32_t n = 0;
        e_sys_log_read(entries, &n, 128);
        int show = c.argc > 0 ? (int)c.integer(0) : 20;
        if (show > (int)n) show = (int)n;
        printf("  System log (last %d of %u):\n", show, n);
        printf("  %-8s %-6s %s\n", "TICK", "TASK", "MESSAGE");
        printf("  %-8s %-6s %s\n", "----", "----", "-------");
        int start = n > (uint32_t)show ? (int)n - show : 0;
        for (int i = start; i < (int)n; i++)
            printf("  %-8llu %-6u %s\n", entries[i].tick, entries[i].source_task, entries[i].text);
    });

    // -------- DEBUG / MONITORING --------
    R.register_cmd("watch", "Watch a command at interval: watch [interval_s] [cmd]", [](const CmdLine& c) {
        printf("  'watch' not implemented as repeating timer yet.\n");
    });
    R.register_cmd("bench", "Quick benchmark test", [](const CmdLine& c) {
        printf("  Running quick benchmark...\n");
        uint64_t t0 = GetTickCount64();
        int loops = c.argc > 0 ? (int)c.integer(0) : 1000;
        if (loops > 100000) loops = 100000;
        float sum = 0;
        for (int i = 0; i < loops; i++) {
            float s[128] = {0}; s[i % 128] = (float)i / loops;
            e_sys_affect_inject(s, 128);
        }
        uint64_t dt = GetTickCount64() - t0;
        printf("  %d affect injects in %llums (%.0f/sec)\n", loops, dt, loops * 1000.0f / (dt ? dt : 1));
    });

    // -------- LEGACY ALIASES --------
    R.register_cmd("stat", "System status (alias for stats)", [](const CmdLine&) {
        auto i = e_sys_get_info();
        printf("  t=%llu tsk=%d/%d sw=%llu mem=%d h=%.3f[%d]\n",
            i.tick, i.active_task_count, i.task_count, i.total_context_switches,
            g_mem_writes, i.hormone_mean, i.hormone_max_idx);
    });
    R.register_cmd("tasks", "Alias for ps", [](const CmdLine& c) {
        uint32_t count = 0; etask_t ids[64];
        e_sys_task_list(ids, &count, 64);
        auto i = e_sys_get_info();
        printf("  Tasks (%d active/%d total):\n", i.active_task_count, i.task_count);
        for (uint32_t j = 0; j < count; j++) {
            etask_info_t ti;
            if (e_sys_task_getinfo(ids[j], &ti) == 0 && ti.name)
                printf("    [%d] %-10s %s p=%d\n", ti.id, ti.name, task_state_str(ti.state), ti.priority);
        }
    });
    
    R.register_cmd("quit", "Shutdown and exit EmotionOS", [](const CmdLine&) {
        printf("  Shutting down...\n");
        e_sys_shutdown();
        e_sys_task_exit(0);
    });
    R.register_cmd("exit", "Alias for quit", [](const CmdLine& c) {
        CmdRegistry::instance().execute("quit");
    });
    R.register_cmd("save", "Flush NEFS to disk (alias for umount+mount)", [](const CmdLine&) {
        if (!g_nefs) { printf("  NEFS not initialized\n"); return; }
        g_nefs->umount();
        g_nefs->mount("D:\\Project\\EmotionOS\\kernel\\nefs.img");
        printf("  NEFS saved and remounted.\n");
    });
} // register_all_commands()
} // namespace shell
} // namespace eos
