// EmotionOS v0.5 Integration Tests
#include "../kernel/include/emotionos/types.h"
#include "../kernel/include/emotionos/syscall.h"
#include "../kernel/include/emotionos/affect/affective_core.h"
#include "../kernel/include/emotionos/memory/stg.h"
#include "../kernel/include/emotionos/memory/ltl.h"
#include "../kernel/include/emotionos/memory/mnemonic.h"
#include "../kernel/include/emotionos/nefs/nefs.h"
#include "../kernel/include/emotionos/security/permissions.h"
#include <cstdio>
#include <cstring>
#include <cmath>
#include <windows.h>

int gp=0,gf=0,gt=0;
#define TN(n) do{gt++;printf("  TEST %-55s ",n);}while(0)
#define OK do{gp++;printf("PASS\n");}while(0)
#define NO(s) do{gf++;printf("FAIL: %s\n",s);}while(0)

static void t1(){ TN("Core create+step"); eos::affect::AffectiveCore c; float s[128]={0}; c.step(s,0); if(c.step_count()==1)OK;else NO("step"); }
static void t2(){ TN("10 steps stability"); eos::affect::AffectiveCore c; float s[128]={0}; for(int n=0;n<10;n++)c.step(s,0); bool ok=(c.step_count()==10); const float* e=c.emotions(); for(int i=0;i<8;i++)if(fabsf(e[i])>1.01f)ok=0; if(ok)OK;else NO("emotion"); }
static void t3(){ TN("Stimulus inject"); eos::affect::AffectiveCore c; float s[128]={0}; for(int i=40;i<60;i++)s[i]=0.9f; c.inject_stimulus(s,128); float out[128]={0}; bool ok=c.consume_pending_stimulus(out); for(int i=40;i<60;i++)if(fabsf(out[i]-0.9f)>0.01f)ok=0; if(ok)OK;else NO("inject"); }
static void t4(){ TN("Sculpt toggle"); eos::affect::AffectiveCore c; bool ok=c.is_sculpting_active(); c.enable_sculpting(0); ok=ok&&!c.is_sculpting_active(); c.enable_sculpting(1); ok=ok&&c.is_sculpting_active(); if(ok)OK;else NO("toggle"); }
static void t5(){ TN("Memory feedback"); eos::affect::AffectiveCore c; float m[128]={0}; for(int i=0;i<16;i++)m[i]=0.5f; c.apply_memory_feedback(m,0.8f); float s[128]={0}; c.step(s,0); if(c.step_count()==1)OK;else NO("fb"); }
static void t6(){ TN("Cortisol response"); eos::affect::AffectiveCore c; float s[128]={0}; for(int i=0;i<16;i++)s[i]=0.95f; for(int n=0;n<40;n++)c.step(s,0); const float* h=c.hormones(); if(h[1]>0.3f)OK;else NO("cortisol"); }
static void t7(){ TN("NEFS format+create"); eos::nefs::NEFS n; n.format(0); int64_t s[]={128,64}; uint32_t id=n.create_at("/t",s,2); bool ok=id>0&&n.tensor_count()==2; if(ok)OK;else NO("format"); }
static void t8(){ TN("NEFS write"); eos::nefs::NEFS n; n.format(0); int64_t s[]={64}; uint32_t id=n.create_at("/d",s,1); int64_t idx[]={10}; bool ok=n.write_entry(id,idx,0.75f,0.5f); if(ok&&n.get_inode(id)->entry_count==1)OK;else NO("write"); }
static void t9(){ TN("NEFS resolve"); eos::nefs::NEFS n; n.format(0); int64_t a1[]={16},a2[]={32}; n.create_at("/x",a1,1); n.create_at("/y",a2,1); bool ok=n.resolve("/x")>0&&n.resolve("/y")>0&&n.resolve("/z")==0; if(ok)OK;else NO("resolve"); }
static void t10(){ TN("NEFS emotion"); eos::nefs::NEFS n; n.format(0); int64_t s[]={64}; uint32_t id=n.create_at("/e",s,1); float emo[8]={0.8f},hor[16]={0.5f}; n.set_emotion(id,emo,hor); auto* d=n.get_inode(id); bool ok=fabsf(d->emotion_profile[0]-0.8f)<0.01f&&fabsf(d->hormone_profile[0]-0.5f)<0.01f; if(ok)OK;else NO("emotion"); }
static void t11(){ TN("NEFS multi-tensor"); eos::nefs::NEFS n; n.format(0); int64_t a1[]={32},a2[]={64},a3[]={8,8}; n.create_at("/a",a1,1); n.create_at("/b",a2,1); n.create_at("/c",a3,2); if(n.tensor_count()==4)OK;else NO("count"); }
static void t12(){ TN("Permission domains"); bool ok=eos::security::check_permission(eos::security::domain_default_permissions(eos::security::DOMAIN_LOGIC),eos::security::PERM_AFFECT_WRITE); ok=ok&&!eos::security::check_permission(eos::security::domain_default_permissions(eos::security::DOMAIN_USER),eos::security::PERM_AFFECT_INJECT); ok=ok&&eos::security::check_permission(eos::security::domain_default_permissions(eos::security::DOMAIN_LOGIC),eos::security::PERM_MEMORY_SCULPT); ok=ok&&!eos::security::check_permission(eos::security::domain_default_permissions(eos::security::DOMAIN_USER),eos::security::PERM_NEFS_FORMAT); if(ok)OK;else NO("perm"); }
static void t13(){ TN("IPC isolation"); bool ok=eos::security::check_permission(eos::security::domain_default_permissions(eos::security::DOMAIN_LOGIC),eos::security::PERM_IPC_BROADCAST); ok=ok&&!eos::security::check_permission(eos::security::domain_default_permissions(eos::security::DOMAIN_USER),eos::security::PERM_IPC_BROADCAST); ok=ok&&eos::security::check_permission(eos::security::domain_default_permissions(eos::security::DOMAIN_USER),eos::security::PERM_IPC_LISTEN); if(ok)OK;else NO("isolation"); }

int main(){
  setvbuf(stdout,0,_IONBF,0);
  printf("=================================\n");
  printf(" EmotionOS v0.5 Integration\n");
  printf("=================================\n\n");
  printf("\n== AffectiveCore ==\n");
  t1();
  t2();
  t3();
  t4();
  t5();
  t6();
  printf("\n== NEFS ==\n");
  t7();
  t8();
  t9();
  t10();
  t11();
  printf("\n== Permissions ==\n");
  t12();
  t13();
  printf("\n\n=================================\n");
  printf(" v0.5: %d p %d f %d t\n",gp,gf,gt);
  printf("=================================\n");
  return gf?1:0;
}
