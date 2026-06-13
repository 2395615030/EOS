// EmotionOS - Shell Engine v1.0
// 命令注册表 + 参数解析 + 管线
#pragma once
#include "../types.h"
#include "../nefs/nefs.h"
#include <cstdio>
#include <cstring>
#include <cstdint>
#include <vector>
#include <functional>

namespace eos { namespace shell {

constexpr uint32_t MAX_ARGS = 32;
constexpr uint32_t CMD_LINE_MAX = 256;
constexpr uint32_t MAX_ALIAS = 64;

enum ArgType : uint8_t {
  ARG_NONE, ARG_INT, ARG_FLOAT, ARG_STRING, ARG_FLAG,
  ARG_PATH,   // NEFS path like /system/affect/state
  ARG_TASK,   // task ID
};
struct CmdArg {
  ArgType type;
  union { int64_t i; float f; };
  char s[CMD_LINE_MAX];
};
struct CmdLine {
  char name[64];
  int argc;
  CmdArg argv[MAX_ARGS];
  bool pipe;  // future: support pipelines
  char raw[CMD_LINE_MAX];

  void clear() { argc = 0; pipe = false; memset(name, 0, sizeof(name)); memset(raw, 0, sizeof(raw)); }
  CmdArg* arg(int i) const { return (i >= 0 && i < argc) ? (CmdArg*)&argv[i] : nullptr; }
  const char* str(int i) const { auto* a = arg(i); return a ? a->s : nullptr; }
  int64_t integer(int i) const { auto* a = arg(i); return a ? a->i : 0; }
  float flt(int i) const { auto* a = arg(i); return a ? a->f : 0.0f; }
};

CmdLine parse(const char* input) {
  CmdLine cmd; cmd.clear();
  if (!input || !input[0]) return cmd;
  strncpy(cmd.raw, input, CMD_LINE_MAX-1);

  char buf[CMD_LINE_MAX];
  strncpy(buf, input, CMD_LINE_MAX-1);

  // First token = command name
  char* tok = strtok(buf, " \t");
  if (!tok) return cmd;
  strncpy(cmd.name, tok, 63);

  // Remaining tokens = args
  while ((tok = strtok(nullptr, " \t")) != nullptr && cmd.argc < MAX_ARGS) {
    auto& a = cmd.argv[cmd.argc];
    strncpy(a.s, tok, CMD_LINE_MAX-1);
    // Try int
    char* end = nullptr;
    long long iv = strtoll(tok, &end, 0);
    if (end && *end == 0 && end != tok) {
      a.type = ARG_INT; a.i = (int64_t)iv;
    } else {
      // Try float
      float fv = strtof(tok, &end);
      if (end && *end == 0 && end != tok) {
        a.type = ARG_FLOAT; a.f = fv;
      } else if (tok[0] == '-' && tok[1] == '-' && tok[2]) {
        // Flag like --flag
        a.type = ARG_FLAG;
      } else {
        a.type = ARG_STRING;
        // Check if it looks like a path
        if (tok[0] == '/' || tok[0] == '.') a.type = ARG_PATH;
      }
    }
    cmd.argc++;
  }
  return cmd;
}

using CmdHandler = std::function<void(const CmdLine&)>;
struct CmdEntry {
  char name[64];
  char help[128];
  CmdHandler handler;
  bool builtin;
};
class CmdRegistry {
public:
  static CmdRegistry& instance() { static CmdRegistry r; return r; }

  void register_cmd(const char* name, const char* help, CmdHandler handler) {
    if (count_ >= MAX_ALIAS) return;
    auto& e = entries_[count_++];
    strncpy(e.name, name, 63); strncpy(e.help, help, 127);
    e.handler = handler; e.builtin = true;
  }

  bool execute(const char* input) {
    if (!input || !input[0] || input[0] == '#') return true;
    CmdLine cmd = parse(input);
    if (!cmd.name[0]) return true;
    for (uint32_t i = 0; i < count_; i++) {
      if (strcmp(entries_[i].name, cmd.name) == 0) {
        entries_[i].handler(cmd);
        return true;
      }
    }
    printf("  Unknown cmd: %s\n", cmd.name);
    return false;
  }

  void print_help() {
    printf("  Commands:\n");
    for (uint32_t i = 0; i < count_; i++)
      printf("    %-16s %s\n", entries_[i].name, entries_[i].help);
  }

  void print_help(const char* filter) const {
    for (uint32_t i = 0; i < count_; i++)
      if (strstr(entries_[i].name, filter) || strstr(entries_[i].help, filter))
        printf("    %-16s %s\n", entries_[i].name, entries_[i].help);
  }

private:
  CmdEntry entries_[MAX_ALIAS];
  uint32_t count_ = 0;
};

#define REGISTER_CMD(name, help, body) \
  CmdRegistry::instance().register_cmd(name, help, [](const CmdLine& cmd) { body });

}} // namespace
