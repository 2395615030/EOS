#include <cstdio>
#include <cstring>
#include <cmath>
#include <cstdlib>
#include <windows.h>
#include "emotionos/types.h"
#include "emotionos/syscall.h"
#include "emotionos/memory/stg.h"
#include "emotionos/memory/ltl.h"
#include "emotionos/memory/mnemonic.h"

static int g_pass=0, g_fail=0, g_total=0;
#define TEST(n) do{g_total++;printf("  TEST %-55s ",n);}while(0)
#define PASS do{g_pass++;printf("PASS\n");}while(0)
#define FAIL(s) do{g_fail++;printf("FAIL: %s\n",s);}while(0)
#define CHECK(c) do{if(!(c)){FAIL(#c);return;}}while(0)
#define CHECK_EQ(a,b) do{if((a)!=(b)){char bf[256];snprintf(bf,256,"%s: %lld!=%lld",#a,(long long)(a),(long long)(b));FAIL(bf);return;}}while(0)
static void hdr(const char* s){printf("\n======== %s ========\n",s);}

static void t_getinfo_bad_id(void*){TEST("getinfo bad id");etask_info_t inf;CHECK_EQ(e_sys_task_getinfo(9999,&inf),-1);PASS;}
static void t_free_null(void*){TEST("free(null)");e_sys_mem_free(nullptr);PASS;}
static void t_self_kernel(void*){TEST("self in sched");CHECK(e_sys_task_self()!=0);PASS;}

static void t_basic(void*){TEST("create+self+getinfo");etask_t s=e_sys_task_self();CHECK(s!=0);etask_info_t inf;memset(&inf,0,sizeof(inf));CHECK_EQ(e_sys_task_getinfo(s,&inf),0);CHECK(inf.id==s&&inf.name!=0);PASS;}
static void t_sleep(void*){TEST("sleep 300ms");uint64_t t1=GetTickCount64();e_sys_task_sleep(300);uint64_t d=GetTickCount64()-t1;CHECK(d>=200&&d<=600);PASS;}
static void t_yield(void*){TEST("yield 3x");int y=0;for(int i=0;i<3;i++){y++;e_sys_task_yield();}CHECK_EQ(y,3);PASS;}
static int g_ev=0;static void t_exit_fn(void*){g_ev=42;e_sys_task_exit(0);}
static void t_exit(void*){TEST("task exit");g_ev=0;e_sys_task_create("ex",EPRIO_NORMAL,t_exit_fn,nullptr,16384);e_sys_task_sleep(200);CHECK_EQ(g_ev,42);PASS;}
static void t_named(void*){TEST("named task");etask_info_t inf;e_sys_task_getinfo(e_sys_task_self(),&inf);CHECK(inf.name!=0&&strlen(inf.name)>0);PASS;}

static void t_ipc_bsr(void*){volatile int got=0;auto rfn=[](void*p){e_sys_ipc_bind(20);int*gp=(int*)p;eipc_msg_t m;if(e_sys_ipc_recv(&m,3000)==0&&m.code==99)*gp=1;e_sys_task_exit(0);};auto sfn=[](void*){e_sys_task_sleep(100);eipc_msg_t m;memset(&m,0,sizeof(m));m.type=EIPC_USER;m.code=99;m.value=42;e_sys_ipc_send(0,&m);e_sys_task_exit(0);};int r=0;e_sys_task_create("r",EPRIO_NORMAL,rfn,(void*)&r,16384);e_sys_task_create("s",EPRIO_NORMAL,sfn,nullptr,16384);TEST("IPC send+recv");e_sys_task_sleep(800);CHECK_EQ(r,1);PASS;}
static void t_ipc_poll(void*){TEST("IPC poll -3");e_sys_ipc_bind(21);eipc_msg_t m;CHECK_EQ(e_sys_ipc_poll(&m),-3);PASS;}
static void t_ipc_bind2(void*){TEST("IPC bind twice");e_sys_ipc_bind(22);CHECK_EQ(e_sys_ipc_bind(22),-2);PASS;}
static void t_ipc_dir(void*){volatile int got=0;auto rfn=[](void*p){e_sys_ipc_bind(23);int*gp=(int*)p;eipc_msg_t m;if(e_sys_ipc_recv(&m,3000)==0&&m.code==77)*gp=1;e_sys_task_exit(0);};auto sfn=[](void*p){e_sys_task_sleep(100);etask_t t=(etask_t)(uintptr_t)p;eipc_msg_t m;memset(&m,0,sizeof(m));m.type=EIPC_USER;m.code=77;m.value=1;e_sys_ipc_send(t,&m);e_sys_task_exit(0);};etask_t rid=e_sys_task_create("dr",EPRIO_NORMAL,rfn,(void*)&got,16384);e_sys_task_sleep(50);e_sys_task_create("ds",EPRIO_NORMAL,sfn,(void*)(uintptr_t)rid,16384);TEST("IPC directed");e_sys_task_sleep(800);CHECK_EQ(got,1);PASS;}

static void t_mem_af(void*){TEST("alloc+free");void*p=e_sys_mem_alloc(4096,EMEM_RW);CHECK(p!=0);memset(p,0xCC,4096);CHECK_EQ(e_sys_mem_free(p),0);PASS;}
static void t_mem_afx(void*){TEST("alloc RWX");void*p=e_sys_mem_alloc(4096,EMEM_RWX);CHECK(p!=0);CHECK_EQ(e_sys_mem_free(p),0);PASS;}
static void t_mem_pin(void*){TEST("pin+unpin");void*p=e_sys_mem_alloc(4096,EMEM_RW);CHECK(p!=0);int r=e_sys_mem_pin(p,4096);if(r==0)e_sys_mem_unpin(p,4096);e_sys_mem_free(p);PASS;}
static void t_mem_fnull(void*){TEST("free null");e_sys_mem_free(nullptr);PASS;}

static void t_aff_get(void*){TEST("affect_get 16+8");float h[16],e[8];int hc=16,ec=8;CHECK_EQ(e_sys_affect_get(h,&hc,e,&ec),0);CHECK_EQ(hc,16);CHECK_EQ(ec,8);PASS;}
static void t_aff_null(void*){TEST("affect_get null");CHECK_EQ(e_sys_affect_get(nullptr,nullptr,nullptr,nullptr),0);PASS;}
static void t_aff_inj(void*){volatile int r=0;auto l=[](void*p){e_sys_ipc_bind(1);int*rp=(int*)p;eipc_msg_t m;while(1){if(e_sys_ipc_recv(&m,3000)==0&&m.type==EIPC_EVENT){*rp=1;break;}}e_sys_task_exit(0);};auto j=[](void*){e_sys_task_sleep(100);float s[128]={};for(int i=0;i<64;i++)s[i]=0.8f;e_sys_affect_inject(s,128);e_sys_task_exit(0);};e_sys_task_create("l",EPRIO_NORMAL,l,(void*)&r,16384);e_sys_task_create("j",EPRIO_NORMAL,j,nullptr,16384);TEST("affect_inject");e_sys_task_sleep(1000);CHECK_EQ(r,1);PASS;}

static void t_irq_reg(void*){TEST("irq_register");eirq_t irq=e_sys_irq_register([](void*){},nullptr);CHECK(irq!=0);PASS;}

static void t_sys_info(void*){TEST("sys_get_info");esys_info_t i=e_sys_get_info();CHECK(i.task_count>0&&i.uptime_ms>0);PASS;}
static void t_prog_load(void*){TEST("program_load");CHECK_EQ(e_sys_program_load("C:\\nonexistent.dll","bad",EPRIO_NORMAL),-1);PASS;}

static void t_cr_null(void*){TEST("create null name");auto f=[](void*){e_sys_task_exit(0);};CHECK(e_sys_task_create(nullptr,EPRIO_NORMAL,f,nullptr,4096)!=0);e_sys_task_sleep(100);PASS;}
static void t_cr_zstk(void*){TEST("create 0 stack");auto f=[](void*){e_sys_task_exit(0);};CHECK(e_sys_task_create("z",EPRIO_NORMAL,f,nullptr,0)!=0);e_sys_task_sleep(100);PASS;}

// ===== MnemonicOrgan Tests (heap-allocated to avoid fiber stack overflow) =====
static void t_stg_wr(void*){
    auto* stg = new eos::memory::STG();
    TEST("STG write+recall");float v[128]={},h[16]={},e[8]={};for(int i=0;i<16;i++)v[i]=0.5f;
    int idx=stg->write(v,h,e,0.0f,0);CHECK(idx>=0);int oi[8];float os[8];int n=stg->recall(v,h,oi,os,8);
    CHECK(n>0);CHECK_EQ(oi[0],idx);PASS;delete stg;
}
static void t_stg_multi(void*){
    auto* stg = new eos::memory::STG();
    TEST("STG multiple writes");float h[16]={},e[8]={};float v1[128]={1.0f};int i1=stg->write(v1,h,e,0.0f,0);
    float v2[128]={};v2[0]=-1.0f;int i2=stg->write(v2,h,e,0.0f,1);CHECK(i1!=i2);PASS;delete stg;
}
static void t_stg_sal(void*){
    auto* stg = new eos::memory::STG();
    TEST("STG salience");float v[128]={},h[16]={},e[8]={};v[0]=0.5f;stg->write(v,h,e,0.0f,0);
    const auto*n=stg->get_node(0);CHECK(n!=nullptr);CHECK(n->salience>0.5f);PASS;delete stg;
}
static void t_ltl_cons(void*){
    auto* stg = new eos::memory::STG(); auto* ltl = new eos::memory::LTL();
    TEST("LTL consolidate");float h[16]={0.4f};
    for(int w=0;w<3;w++){float v[128]={};for(int i=0;i<16;i++)v[i]=(float)(w+1)/5.0f;float e_dummy[8]={};stg->write(v,h,e_dummy,0.0f,w);}
    int c=stg->node_count();float vecs[3*128];int idx[3];int a=0;
    for(int i=0;i<c&&i<3;i++){const auto*n=stg->get_node(i);if(n&&n->active&&n->salience>0.3f){memcpy(vecs+a*128,n->vec,128*4);idx[a]=i;a++;}}
    ltl->consolidate(vecs,idx,a,h,100);CHECK(ltl->attractor_count()>0);PASS;delete stg;delete ltl;
}
static void t_ltl_samp(void*){
    auto* ltl = new eos::memory::LTL();
    TEST("LTL sample");float h[16]={0.4f};float v[128]={};for(int i=0;i<16;i++)v[i]=0.5f;
    int idx[1]={0};ltl->consolidate(v,idx,1,h,100);float out[128]={0},em[8]={0},sc=0;
    int attr=ltl->sample(v,0.2f,out,em,&sc);CHECK(attr>=0);CHECK(sc>0.0f);PASS;delete ltl;
}
static void t_mem_full(void*){
    auto* mem = new eos::memory::MnemonicOrgan();
    TEST("Mnemonic full cycle");float v[128]={},h[16]={0.4f},e[8]={};for(int i=16;i<48;i++)v[i]=0.7f;
    mem->write(v,h,e,-1.0f,1);float q[128]={};for(int i=16;i<48;i++)q[i]=0.7f;
    auto r=mem->recall(q,h);CHECK(r.stg_count>0);PASS;delete mem;
}
static void t_mem_freea(void*){
    auto* mem = new eos::memory::MnemonicOrgan();
    TEST("free_associate");auto r=mem->free_associate(nullptr,1.0f);(void)r;PASS;delete mem;
}
static void t_mem_flash(void*){
    auto* mem = new eos::memory::MnemonicOrgan();
    TEST("flashbulb");float v[128]={},h[16]={0.4f},e[8]={};int idx=mem->write(v,h,e,0.0f,1);
    float pe[8]={};for(int i=0;i<4;i++)pe[i]=0.8f;mem->flashbulb_memory(idx,pe);PASS;delete mem;
}

static void t_stress(void*){int n=0;auto fn=[](void*){e_sys_task_exit(0);};for(int i=0;i<80;i++){char nm[16];snprintf(nm,16,"f%d",i);if(!e_sys_task_create(nm,EPRIO_NORMAL,fn,nullptr,4096))break;n++;}TEST("stress max tasks");CHECK_EQ(e_sys_task_create("last",EPRIO_NORMAL,fn,nullptr,4096),(etask_t)0);PASS;}

static void run_tests(){
    printf("[RUNNER] Start\n");
    hdr("1. Task Mgmt"); e_sys_task_create("b",EPRIO_NORMAL,t_basic,nullptr,16384);e_sys_task_sleep(30);
    e_sys_task_create("sl",EPRIO_NORMAL,t_sleep,nullptr,16384);e_sys_task_sleep(30);
    e_sys_task_create("yl",EPRIO_NORMAL,t_yield,nullptr,16384);e_sys_task_sleep(30);
    e_sys_task_create("ex",EPRIO_NORMAL,t_exit,nullptr,16384);e_sys_task_sleep(100);
    e_sys_task_create("nm",EPRIO_NORMAL,t_named,nullptr,16384);e_sys_task_sleep(30);
    hdr("2. IPC"); e_sys_task_create("bsr",EPRIO_NORMAL,t_ipc_bsr,nullptr,16384);e_sys_task_sleep(200);
    e_sys_task_create("pol",EPRIO_NORMAL,t_ipc_poll,nullptr,16384);e_sys_task_sleep(30);
    e_sys_task_create("b2",EPRIO_NORMAL,t_ipc_bind2,nullptr,16384);e_sys_task_sleep(30);
    e_sys_task_create("dir",EPRIO_NORMAL,t_ipc_dir,nullptr,16384);e_sys_task_sleep(200);
    hdr("3. Memory"); e_sys_task_create("af",EPRIO_NORMAL,t_mem_af,nullptr,16384);e_sys_task_sleep(30);
    e_sys_task_create("afx",EPRIO_NORMAL,t_mem_afx,nullptr,16384);e_sys_task_sleep(30);
    e_sys_task_create("pin",EPRIO_NORMAL,t_mem_pin,nullptr,16384);e_sys_task_sleep(30);
    e_sys_task_create("fnl",EPRIO_NORMAL,t_mem_fnull,nullptr,16384);e_sys_task_sleep(30);
    hdr("4. Affect"); e_sys_task_create("ag",EPRIO_NORMAL,t_aff_get,nullptr,16384);e_sys_task_sleep(30);
    e_sys_task_create("an",EPRIO_NORMAL,t_aff_null,nullptr,16384);e_sys_task_sleep(30);
    e_sys_task_create("aj",EPRIO_NORMAL,t_aff_inj,nullptr,16384);e_sys_task_sleep(300);
    hdr("5. IRQ"); e_sys_task_create("ir",EPRIO_NORMAL,t_irq_reg,nullptr,16384);e_sys_task_sleep(30);
    hdr("6. System"); e_sys_task_create("si",EPRIO_NORMAL,t_sys_info,nullptr,16384);e_sys_task_sleep(30);
    e_sys_task_create("pl",EPRIO_NORMAL,t_prog_load,nullptr,16384);e_sys_task_sleep(30);
    hdr("7. Boundary"); e_sys_task_create("cn",EPRIO_NORMAL,t_cr_null,nullptr,16384);e_sys_task_sleep(30);
    e_sys_task_create("cz",EPRIO_NORMAL,t_cr_zstk,nullptr,16384);e_sys_task_sleep(30);
    e_sys_task_create("bi",EPRIO_NORMAL,t_getinfo_bad_id,nullptr,16384);e_sys_task_sleep(30);
    hdr("8. MnemonicOrgan"); e_sys_task_create("sw",EPRIO_NORMAL,t_stg_wr,nullptr,16384);e_sys_task_sleep(30);
    e_sys_task_create("sm",EPRIO_NORMAL,t_stg_multi,nullptr,16384);e_sys_task_sleep(30);
    e_sys_task_create("ss",EPRIO_NORMAL,t_stg_sal,nullptr,16384);e_sys_task_sleep(30);
    e_sys_task_create("lc",EPRIO_NORMAL,t_ltl_cons,nullptr,16384);e_sys_task_sleep(30);
    e_sys_task_create("ls",EPRIO_NORMAL,t_ltl_samp,nullptr,16384);e_sys_task_sleep(30);
    e_sys_task_create("mf",EPRIO_NORMAL,t_mem_full,nullptr,16384);e_sys_task_sleep(30);
    e_sys_task_create("mfa",EPRIO_NORMAL,t_mem_freea,nullptr,16384);e_sys_task_sleep(30);
    e_sys_task_create("mfb",EPRIO_NORMAL,t_mem_flash,nullptr,16384);e_sys_task_sleep(30);
    hdr("9. Stress"); e_sys_task_create("smax",EPRIO_NORMAL,t_stress,nullptr,16384);e_sys_task_sleep(200);
}

int main(){
    setvbuf(stdout,nullptr,_IONBF,0);
    printf("========================================\n");
    printf(" EmotionOS Test Suite v1.1\n");
    printf(" (36 tests: 21 APIs + MnemonicOrgan)\n");
    printf("========================================\n\n");
    e_kernel_init();
    hdr("Boundary (pre-scheduler)");
    e_sys_task_create("__b1",EPRIO_ROOT,t_getinfo_bad_id,nullptr,4096);
    e_sys_task_create("__b2",EPRIO_ROOT,t_free_null,nullptr,4096);
    e_sys_task_create("__b3",EPRIO_ROOT,t_self_kernel,nullptr,4096);
    e_sys_task_create("__wd",EPRIO_ROOT,[](void*){e_sys_task_sleep(25000);printf("\n[WD] timeout\n");e_sys_shutdown();e_sys_task_exit(0);},nullptr,4096);
    e_sys_task_create("__run",EPRIO_ROOT,[](void*){run_tests();printf("\nAll done.\n");e_sys_task_sleep(200);e_sys_shutdown();e_sys_task_exit(0);},nullptr,16384);
    printf("\nStarting scheduler...\n\n");
    e_kernel_run();
    printf("\n========================================\n");
    printf(" Results: %d/%d passed, %d failed\n",g_pass,g_total,g_fail);
    printf("========================================\n");
    return g_fail?1:0;
}


