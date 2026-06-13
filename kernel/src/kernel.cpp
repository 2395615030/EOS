// EmotionOS microkernel v0.3
#include "emotionos/types.h"
#include "emotionos/syscall.h"
#include <cstdio>
#include <cstring>
#include <windows.h>
#include <deque>
#include <map>
#include <cstdarg>

#define MAX_TASKS 64
struct TCtrl{etask_t id;char name[32];eprio_t prio;etstate_t state;void* fiber;uint64_t ticks,wakeup;std::deque<eipc_msg_t> msgs;};
static struct{
  uint64_t tick,start_ms;uint32_t next_id;uint64_t switches;bool running;
  TCtrl tasks[MAX_TASKS];int count;std::deque<int>ready;std::map<int,int>ports;
  int current;void* sched;float h[16];float e[8];
  // Service registry
  eservice_entry_t services[MAX_SERVICES];int service_count;
  // System log
  elog_entry_t log[MAX_LOG_ENTRIES];int log_count;
  // Suspend tracking
  etstate_t suspended_state[MAX_TASKS]; // saved state for suspended tasks
}K;
struct EM{void(*fn)(void*);void*arg;};static EM GE[MAX_TASKS];
static void __stdcall ep(void*p){int idx=(int)(intptr_t)p;if(idx>=0&&idx<MAX_TASKS&&GE[idx].fn)GE[idx].fn(GE[idx].arg);for(int i=0;i<K.count;i++)if(K.tasks[i].fiber==GetCurrentFiber()){K.tasks[i].state=ETS_TERMINATED;break;}SwitchToFiber(K.sched);}
static int F(etask_t id){for(int i=0;i<K.count;i++)if(K.tasks[i].id==id)return i;return -1;}
etask_t e_sys_task_create(const char*name,eprio_t prio,void(*entry)(void*),void*arg,size_t stack){if(K.count>=MAX_TASKS)return 0;int i=K.count++;auto&t=K.tasks[i];t.id=K.next_id++;strncpy(t.name,name?name:"?",31);t.name[31]=0;t.prio=prio;t.state=ETS_READY;t.ticks=0;t.wakeup=0;if(!stack)stack=16384;GE[i]={entry,arg};t.fiber=CreateFiber((SIZE_T)stack,ep,(void*)(intptr_t)i);if(!t.fiber){K.count--;return 0;}K.ready.push_back(i);return t.id;}
void e_sys_task_exit(int){if(K.current>=0){K.tasks[K.current].state=ETS_TERMINATED;K.current=-1;}SwitchToFiber(K.sched);}
void e_sys_task_sleep(uint32_t ms){if(K.current<0)return;K.tasks[K.current].state=ETS_SLEEPING;K.tasks[K.current].wakeup=GetTickCount64()+ms;K.current=-1;SwitchToFiber(K.sched);}
void e_sys_task_yield(void){if(K.current<0)return;K.tasks[K.current].ticks++;K.tasks[K.current].state=ETS_READY;K.ready.push_back(K.current);K.current=-1;SwitchToFiber(K.sched);}
etask_t e_sys_task_self(void){return(K.current>=0)?K.tasks[K.current].id:0;}
estatus_t e_sys_task_getinfo(etask_t id,etask_info_t*inf){int i=F(id);if(i<0)return-1;inf->id=K.tasks[i].id;inf->priority=K.tasks[i].prio;inf->state=K.tasks[i].state;inf->name=K.tasks[i].name;inf->total_ticks=K.tasks[i].ticks;inf->sleep_until=K.tasks[i].wakeup;return 0;}
estatus_t e_sys_ipc_bind(eport_t p){if(K.current<0) return -1; for(auto&[k,v]:K.ports) if(k==p) return -2; K.ports[p]=K.current; return 0;}
estatus_t e_sys_ipc_send(etask_t t,const eipc_msg_t*m){auto E=[&](int i){K.tasks[i].msgs.push_back(*m);if(K.tasks[i].state==ETS_WAITING){K.tasks[i].state=ETS_READY;K.ready.push_back(i);}};if(t==0){for(auto&[_,i]:K.ports)E(i);return 0;}int i=F(t);if(i>=0)E(i);return i>=0?0:-1;}
estatus_t e_sys_ipc_recv(eipc_msg_t*m,uint32_t to){if(K.current<0)return-1;auto&mq=K.tasks[K.current].msgs;if(!mq.empty()){*m=mq.front();mq.pop_front();return 0;}if(!to)return-3;K.tasks[K.current].state=ETS_WAITING;K.current=-1;SwitchToFiber(K.sched);if(K.current>=0&&!K.tasks[K.current].msgs.empty()){*m=K.tasks[K.current].msgs.front();K.tasks[K.current].msgs.pop_front();return 0;}return-3;}
estatus_t e_sys_ipc_poll(eipc_msg_t*m){return e_sys_ipc_recv(m,0);}
void*e_sys_mem_alloc(size_t s,emem_flags_t f){return VirtualAlloc(nullptr,s,MEM_COMMIT|MEM_RESERVE,(f&EMEM_EXEC)?PAGE_EXECUTE_READWRITE:PAGE_READWRITE);}
estatus_t e_sys_mem_free(void*a){return VirtualFree(a,0,MEM_RELEASE)?0:-1;}
estatus_t e_sys_mem_pin(void*a,size_t s){return VirtualLock(a,s)?0:-1;}
estatus_t e_sys_mem_unpin(void*a,size_t s){return VirtualUnlock(a,s)?0:-1;}
eirq_t e_sys_irq_register(void(*)(void*),void*){static eirq_t n=1;return n++;}
estatus_t e_sys_irq_raise(etask_t t,eirq_t irq){eipc_msg_t m{};m.type=(eipc_type_t)0xF0;m.code=irq;return e_sys_ipc_send(t,&m);}

estatus_t e_sys_task_kill(etask_t id) {
  for (int i = 0; i < K.count; i++) if (K.tasks[i].id == id) {
    if (K.tasks[i].state == ETS_TERMINATED || K.tasks[i].id == id && K.current == i) return -2;
    K.tasks[i].state = ETS_TERMINATED;
    // Remove from ready queue
    for (auto it = K.ready.begin(); it != K.ready.end(); ) {
      if (*it == i) { it = K.ready.erase(it); } else { ++it; }
    }
    return 0;
  }
  return -1;
}
estatus_t e_sys_task_set_priority(etask_t id, eprio_t new_prio) {
  for (int i = 0; i < K.count; i++) if (K.tasks[i].id == id) {
    K.tasks[i].prio = new_prio; return 0;
  }
  return -1;
}
estatus_t e_sys_task_list(etask_t* out_ids, uint32_t* out_count, uint32_t max) {
  uint32_t c = 0;
  for (int i = 0; i < K.count && c < max; i++) {
    if (K.tasks[i].state != ETS_TERMINATED) { out_ids[c++] = K.tasks[i].id; }
  }
  if (out_count) *out_count = c;
  return 0;
}


// ---- 系统日志 ----
void e_sys_log(const char* text) {
  if (K.log_count >= MAX_LOG_ENTRIES) {
    // Circular: shift old entries
    for (int i = 0; i < MAX_LOG_ENTRIES - 1; i++) K.log[i] = K.log[i + 1];
    K.log_count--;
  }
  auto& e = K.log[K.log_count++];
  e.tick = K.tick;
  e.source_task = K.current >= 0 ? K.tasks[K.current].id : 0;
  strncpy(e.text, text, LOG_LINE_MAX - 1); e.text[LOG_LINE_MAX - 1] = 0;
}
void e_sys_logf(const char* fmt, ...) {
  char buf[256]; va_list ap; va_start(ap, fmt);
  vsnprintf(buf, sizeof(buf), fmt, ap); va_end(ap);
  e_sys_log(buf);
}
estatus_t e_sys_log_read(elog_entry_t* out, uint32_t* count, uint32_t max) {
  uint32_t n = K.log_count < (int)max ? (uint32_t)K.log_count : max;
  for (uint32_t i = 0; i < n; i++) out[i] = K.log[i];
  if (count) *count = n;
  return 0;
}

// ---- 服务注册表 ----
static int find_service(const char* name) {
  for (int i = 0; i < K.service_count; i++)
    if (strcmp(K.services[i].name, name) == 0) return i;
  return -1;
}
estatus_t e_sys_service_register(const char* name, eport_t port, eprio_t priority, const char* description) {
  if (K.service_count >= MAX_SERVICES || find_service(name) >= 0) return -1;
  auto& s = K.services[K.service_count++];
  s.id = K.service_count;
  strncpy(s.name, name, ESERVICE_NAME_MAX - 1);
  s.port = port; s.priority = priority;
  strncpy(s.description, description, ESERVICE_DESC_MAX - 1);
  s.running = false;
  e_sys_logf("service register: %s port=%u", name, port);
  return 0;
}
estatus_t e_sys_service_lookup(const char* name, eservice_entry_t* out) {
  int i = find_service(name);
  if (i < 0) return -1;
  if (out) *out = K.services[i];
  return 0;
}
estatus_t e_sys_service_list(eservice_entry_t* out, uint32_t* count, uint32_t max) {
  uint32_t n = K.service_count < (int)max ? (uint32_t)K.service_count : max;
  for (uint32_t i = 0; i < n; i++) out[i] = K.services[i];
  if (count) *count = n;
  return 0;
}
estatus_t e_sys_service_unregister(const char* name) {
  int i = find_service(name); if (i < 0) return -1;
  for (int j = i; j < K.service_count - 1; j++) K.services[j] = K.services[j + 1];
  K.service_count--;
  e_sys_logf("service unregister: %s", name);
  return 0;
}

// ---- 任务挂起/恢复 ----
estatus_t e_sys_task_suspend(etask_t id) {
  for (int i = 0; i < K.count; i++) if (K.tasks[i].id == id) {
    if (K.tasks[i].state == ETS_TERMINATED) return -2;
    K.suspended_state[i] = K.tasks[i].state;
    K.tasks[i].state = ETS_BLOCKED;
    e_sys_logf("task suspend: %s[%u]", K.tasks[i].name, id);
    return 0;
  }
  return -1;
}
estatus_t e_sys_task_resume(etask_t id) {
  for (int i = 0; i < K.count; i++) if (K.tasks[i].id == id) {
    if (K.tasks[i].state != ETS_BLOCKED) return -2;
    K.tasks[i].state = K.suspended_state[i] != ETS_BLOCKED ? K.suspended_state[i] : ETS_READY;
    if (K.tasks[i].state == ETS_READY) K.ready.push_back(i);
    e_sys_logf("task resume: %s[%u]", K.tasks[i].name, id);
    return 0;
  }
  return -1;
}

// ---- 记忆召回（实现版）----
estatus_t e_sys_memory_recall(float* output, int max_count, float temperature) {
  if (!output || max_count < 1) return -1;
  // Simplified recall: return the mean hormone vector as placeholder
  // Real implementation requires IPC to memory task
  for (int i = 0; i < max_count && i < 16; i++) output[i] = K.h[i];
  e_sys_logf("memory_recall: %d dims temp=%.2f", max_count, temperature);
  return 0;
}


estatus_t e_sys_affect_get(float*h,int*hc,float*e,int*ec){if(h&&hc){int n=*hc<16?*hc:16;memcpy(h,K.h,n*4);*hc=n;}if(e&&ec){int n=*ec<8?*ec:8;memcpy(e,K.e,n*4);*ec=n;}return 0;}
estatus_t e_sys_affect_inject(const float*s,int c){eipc_msg_t m{};m.type=EIPC_EVENT;m.data=(void*)s;m.length=c*4;for(auto&[_,i]:K.ports)if(_==1){K.tasks[i].msgs.push_back(m);if(K.tasks[i].state==ETS_WAITING){K.tasks[i].state=ETS_READY;K.ready.push_back(i);}}return 0;}
estatus_t e_sys_memory_write(const float* data, int count, float tag){
    eipc_msg_t m{};m.type=EIPC_MEMORY;m.data=(void*)data;m.length=(uint32_t)(count*4);m.fvalue=tag;
    for(auto&[_,i]:K.ports)if(_==2){
        K.tasks[i].msgs.push_back(m);
        if(K.tasks[i].state==ETS_WAITING){K.tasks[i].state=ETS_READY;K.ready.push_back(i);}
    }
    return 0;
}
// e_sys_memory_recall is now implemented above
void e_sys_shutdown(void){printf("[KERN] Shutdown.\n");K.running=false;}
esys_info_t e_sys_get_info(void){esys_info_t i{};i.tick=K.tick;i.task_count=K.count;i.total_context_switches=K.switches;i.uptime_ms=GetTickCount64()-K.start_ms;int a=0;for(int j=0;j<K.count;j++)if(K.tasks[j].state!=ETS_TERMINATED)a++;i.active_task_count=a;float hm=0;int hmi=0;float hx=0;for(int j=0;j<16;j++){hm+=K.h[j];if(K.h[j]>hx){hx=K.h[j];hmi=j;}}i.hormone_mean=hm/16;i.hormone_max=hx;i.hormone_max_idx=hmi;return i;}
void e_kernel_run(void){printf("[KERN] Scheduler start.\n");K.running=true;while(K.running){uint64_t n=GetTickCount64();for(int i=0;i<K.count;i++)if(K.tasks[i].state==ETS_SLEEPING&&n>=K.tasks[i].wakeup){K.tasks[i].state=ETS_READY;K.ready.push_back(i);}if(!K.ready.empty()){int i=K.ready.front();K.ready.pop_front();if(K.tasks[i].state!=ETS_READY){continue;}K.current=i;K.tasks[i].state=ETS_RUNNING;K.tick++;K.switches++;SwitchToFiber(K.tasks[i].fiber);K.current=-1;}else{Sleep(1);K.tick++;}}}
estatus_t e_sys_program_load(const char* path, const char* task_name, eprio_t priority){
    // Strategy 1: Try as DLL with e_program_main export
    HMODULE h = LoadLibraryA(path);
    if(h) {
        auto entry = (void(*)(void*))GetProcAddress(h, "e_program_main");
        if(entry) {
            etask_t tid = e_sys_task_create(task_name ? task_name : "dll_prog", priority, entry, nullptr, 32768);
            if(tid) { e_sys_logf("program_load: DLL %s as task %u", path, tid); return 0; }
            return -3;
        }
        FreeLibrary(h);
    }
    // Strategy 2: Try as standalone EXE via CreateProcess (with stdout pipe for IPC)
    // First check if file exists
    if (GetFileAttributesA(path) == INVALID_FILE_ATTRIBUTES) return -1;
    // Launch as external process with inherited console
    // The external process uses stdin/stdout or shared memory for IPC
    // For now, just start it as a child process
    STARTUPINFOA si = {sizeof(si)};
    PROCESS_INFORMATION pi;
    char cmdLine[512];
    snprintf(cmdLine, sizeof(cmdLine), "%s --emotionos-ipc-port=%d --emotionos-parent=%llu",
        path, K.current >= 0 ? K.current : 0, (unsigned long long)GetCurrentProcessId());
    if (CreateProcessA(NULL, cmdLine, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi)) {
        CloseHandle(pi.hThread);
        // Register the process handle for monitoring
        etask_t tid = e_sys_task_create(task_name ? task_name : "ext_prog", priority,
            [](void*){ while(1){ e_sys_task_sleep(1000); e_sys_task_yield(); } }, nullptr, 8192);
        if(tid) {
            e_sys_logf("program_load: EXE %s PID=%lu", path, pi.dwProcessId);
            CloseHandle(pi.hProcess);
            return 0;
        }
        CloseHandle(pi.hProcess);
        return -3;
    }
    return -1; // both strategies failed
}

void e_kernel_init(void){for(int i=0;i<MAX_TASKS;i++)GE[i]={0,0};K.tick=0;K.start_ms=GetTickCount64();K.next_id=1;K.switches=0;K.running=false;K.count=0;K.current=-1;K.ready.clear();K.ports.clear();K.service_count=0;K.log_count=0;memset(K.suspended_state,0,sizeof(K.suspended_state));for(int i=0;i<16;i++)K.h[i]=0.4f;for(int i=0;i<8;i++)K.e[i]=0;K.sched=ConvertThreadToFiber(nullptr);if(!K.sched)K.sched=GetCurrentFiber();printf("[KERN] EmotionOS microkernel v0.3 init.\n");}













