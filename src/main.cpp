// EmotionOS v0.7 — Shell Engine + NEFS Memory Persistence
#include <cstdio>
#include <cstring>
#include <cmath>
#include <random>
#include <windows.h>
#ifdef __clang__
#include <conio.h>
#else
#include <conio.h>
#endif
#include "emotionos/types.h"
#include "emotionos/syscall.h"
#include "emotionos/memory/stg.h"
#include "emotionos/memory/ltl.h"
#include "emotionos/memory/mnemonic.h"
#include "emotionos/affect/affective_core.h"
#include "emotionos/nefs/nefs.h"
#include "emotionos/security/permissions.h"
#include "emotionos/shell/shell.h"
#include "emotionos/shell/shell_cmds.h"

static std::mt19937 R(42);

// ---- Core System ----
eos::affect::AffectiveCore g_affect;
eos::memory::MnemonicOrgan* g_mem = nullptr;
int g_mem_writes = 0;
eos::nefs::NEFS* g_nefs = nullptr;

static void snapshot_memory_to_nefs() {
  if (!g_mem || !g_nefs) return;
  if (g_mem->stg_node_count() == 0) return;
  // Write STG node count to /system/memory/stg_backup
  int nodes = g_mem->stg_node_count();
  int64_t idx[] = {0};
  g_nefs->write_entry(8, idx, (float)nodes, 0.0f);
}

static void snapshot_affect_to_nefs() {
  if (!g_nefs) return;
  auto* in = g_nefs->get_inode(7);  // /system/affect/state
  if (!in) return;
  const float* h = g_affect.hormones();
  const float* e = g_affect.emotions();
  int64_t idx[] = {(int64_t)g_affect.step_count()};
  float val = 0; for (int i = 0; i < 16; i++) val += h[i]; val /= 16.0f;
  g_nefs->write_entry(7, idx, val, e[0]);
  g_nefs->set_emotion(7, e, h);
}

static void affect_entry(void*){
  printf("[AFFECT] v0.7 start.\n");
  e_sys_ipc_bind(1); e_sys_log("affect: bound to port 1");
  float S[128] = {0};
  uint64_t pt = 0, snap_tick = 0;
  while(1){
    eipc_msg_t m;
    if (e_sys_ipc_poll(&m) == 0 && m.type == EIPC_EVENT && m.data) {
      int n = (m.length/4 < 128) ? m.length/4 : 128;
      memcpy(S, m.data, n * sizeof(float));
      g_affect.inject_stimulus(S, n);
    }
    float stimulus[128] = {0};
    if (!g_affect.consume_pending_stimulus(stimulus))
      for (int i = 0; i < 128; i++) stimulus[i] = (float)(R()%1000)/500.0f-1.0f;
    g_affect.set_tick(GetTickCount64());
    g_affect.step(stimulus, nullptr);
    if (g_affect.step_count() - pt >= 60) { g_affect.print(); pt = g_affect.step_count(); }
    // Snapshot affect to NEFS every 120 steps
    if (g_affect.step_count() - snap_tick >= 120) { snapshot_affect_to_nefs(); snap_tick = g_affect.step_count(); }
    e_sys_task_sleep(50); e_sys_task_yield();
  }
}
static void memory_entry(void*){
  printf("[MEMORY] v0.7 start.\n");
  e_sys_ipc_bind(2); e_sys_log("memory: bound to port 2");
  auto* mem = new eos::memory::MnemonicOrgan(); g_mem = mem;
  float h[16] = {0.4f}, e[8] = {0}; uint64_t tick = 0;
  while(1){
    eipc_msg_t m; int r = e_sys_ipc_poll(&m);
    int hc = 16, ec = 8; e_sys_affect_get(h, &hc, e, &ec); tick++; mem->set_tick(tick);
    if (r == 0 && m.type == EIPC_MEMORY && m.data && m.length >= 4) {
      float vec[128] = {0}; int n = (m.length/4 < 128) ? m.length/4 : 128;
      memcpy(vec, m.data, n * sizeof(float));
      int idx = mem->write(vec, h, e, m.fvalue, tick); g_mem_writes++;
      float emax = 0; for (int i = 0; i < 8; i++) if (fabsf(e[i]) > emax) emax = fabsf(e[i]);
      if (emax > 0.7f) { mem->flashbulb_memory(idx, e); printf("[MEM] Flashbulb!\n"); }
    }
    mem->consolidate_tick(h); mem->update_temperatures(h);
    // Snapshot STG to NEFS every 50 ticks
    if (tick % 50 == 0) snapshot_memory_to_nefs();
    if (tick % 80 == 0) { printf("[MEM] T=%llu W=%d ", tick, g_mem_writes); mem->debug_print(); }
    e_sys_task_sleep(50); e_sys_task_yield();
  }
}
static void nefs_entry(void*){
  printf("[NEFS] v1.0 start.\n");
  auto* nefs = new eos::nefs::NEFS(); g_nefs = nefs;
  nefs->mount("D:\\Project\\EmotionOS\\kernel\\nefs.img");
  // Ensure directories exist
  nefs->mkdir("/system"); nefs->mkdir("/system/memory"); nefs->mkdir("/system/affect");
  nefs->mkdir("/user"); nefs->mkdir("/user/tensors");
  e_sys_service_register("eos.nefs.storage", 7, EPRIO_NORMAL, "NEFS storage service");

  nefs->create_at("/system/affect/state", ((int64_t[]){256,8}), 2);
  nefs->create_at("/system/memory/stg_backup", ((int64_t[]){2048,128}), 2);
  nefs->create_at("/system/affect/emotion_log", ((int64_t[]){256,8}), 2);
  nefs->create_at("/user/tensors/sample", ((int64_t[]){16}), 1);
  nefs->print_tree();
  uint64_t tick = 0;
  while(1){ tick++; e_sys_task_sleep(5000);
    if (tick % 6 == 0) { printf("[NEFS] "); nefs->print_status(); }
    e_sys_task_yield();
  }
}
static void register_commands() {
  eos::shell::register_all_commands();
}

static void sh(void*){
  printf("[SHELL] v0.7. Type 'help' for commands.\n");
  register_commands();
  char b[128]; int p=0; memset(b,0,sizeof(b));
  bool piped = false;
  HANDLE hStd = GetStdHandle(STD_INPUT_HANDLE);
  DWORD mode;
  piped = !GetConsoleMode(hStd, &mode);
  if(!piped) printf("> "); fflush(stdout);
  while(1){
    int got=0;
    if(piped) {
      if(fgets(b, 126, stdin)) {
        size_t len = strlen(b);
        // Skip UTF-8 BOM
        if (len >= 3 && (unsigned char)b[0]==0xEF && (unsigned char)b[1]==0xBB && (unsigned char)b[2]==0xBF) {
          memmove(b, b+3, len-2); len -= 3;
        }
        while(len>0 && (b[len-1]=='\n'||b[len-1]=='\r')) b[--len]=0;
        p=(int)len; got=(p>0)?1:0;
      } else {
        e_sys_task_sleep(500);
        e_sys_shutdown(); e_sys_task_exit(0);
      }
    } else {
      if(_kbhit()){
        char ch=(char)_getch();
        if(ch==13||ch==10){b[p]=0;got=(p>0)?1:0;}
        else if(ch==8&&p>0){p--;b[p]=0;}
        else if(ch>31&&p<127){b[p++]=ch;}
      }
    }
    if(got){
      eos::shell::CmdRegistry::instance().execute(b);
      if(!piped) printf("> ");fflush(stdout);p=0;memset(b,0,sizeof(b));
    }
    e_sys_task_sleep(100);
  }
}static void idle(void*){while(1){Sleep(10);e_sys_task_yield();}}
static void mon(void*){
  printf("[MONITOR] Start.\n"); e_sys_ipc_bind(0);
  while(1){e_sys_task_sleep(5000);auto i=e_sys_get_info();
    printf("\n=== EmotionOS v0.7 ===\nT=%llu T=%d/%d M=%d\n",
      i.tick,i.active_task_count,i.task_count,g_mem_writes);
    const float* e=g_affect.emotions();
    if(e){const char* en[]={"Joy","Sad","Ang","Fear","Surp","Disg","Trst","Antc"};
      int emax=0;for(int i=1;i<8;i++)if(fabsf(e[i])>fabsf(e[emax]))emax=i;
      printf("Emotion: %s %+.3f | H=%.2f\n",en[emax],e[emax],g_affect.hormones()[0]);
    }
    printf("Mem: STG=%d LTL=%d | NEFS: %d tensors\n",
      g_mem?g_mem->stg_node_count():0,g_mem?g_mem->ltl_attractor_count():0,
      g_nefs?g_nefs->inode_count():0);
    printf("===========================\n");
  }
}
static void sig(void*){
  printf("[SIGNAL] Start.\n");int c=0;
  while(1){e_sys_task_sleep(4000);c++;
    float st[128]={};
    if(c%2==0){for(int i=0;i<16;i++)st[i]=0.9f;printf("[EVENT] DANGER\n");}
    else{for(int i=48;i<64;i++)st[i]=0.85f;printf("[EVENT] REWARD\n");}
    e_sys_affect_inject(st,128);e_sys_memory_write(st,128,c%2==0?1.0f:0.0f);
  }
}
int main(int a,char**v){
  setvbuf(stdout,NULL,_IONBF,0);
  printf("=========================================\n");
  printf(" EmotionOS v0.7\n");
  printf("  Shell Complete + Full Filesystem\n");
  printf("=========================================\n\n");
  e_kernel_init();
  e_sys_task_create("idle",EPRIO_IDLE,idle,nullptr,4096);
  e_sys_task_create("affect",EPRIO_HIGH,affect_entry,nullptr,32768);
  e_sys_task_create("memory",EPRIO_HIGH,memory_entry,nullptr,65536);
  e_sys_task_create("nefs",EPRIO_NORMAL,nefs_entry,nullptr,16384);
  e_sys_task_create("monitor",EPRIO_NORMAL,mon,nullptr,16384);
  e_sys_task_create("signal",EPRIO_NORMAL,sig,nullptr,16384);
  e_sys_task_create("shell",EPRIO_LOW,sh,nullptr,16384);
  // Init system: register built-in services
  e_sys_service_register("eos.kernel", 0, EPRIO_ROOT, "EmotionOS microkernel");
  e_sys_service_register("eos.affect", 1, EPRIO_HIGH, "AffectiveCore HPA axis");
  e_sys_service_register("eos.memory", 2, EPRIO_HIGH, "MnemonicOrgan STG+LTL");
  e_sys_service_register("eos.nefs", 3, EPRIO_NORMAL, "NEFS persistent filesystem");
  e_sys_service_register("eos.monitor", 4, EPRIO_NORMAL, "System monitor");
  e_sys_service_register("eos.signal", 5, EPRIO_NORMAL, "External signal injector");
  e_sys_service_register("eos.shell", 6, EPRIO_LOW, "Shell interface");
  e_sys_log("init: 7 core services registered");
  printf("[INIT] 7 core services registered\n");
  printf("\n[BOOT] 7 components\n\n");
  e_kernel_run();
  printf("[BOOT] Halt.\n");
  return 0;
}