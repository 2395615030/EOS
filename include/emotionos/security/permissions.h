// ============================================================
// EmotionOS - SafetyPermission 三重权限模型
// Root/Logic/User 权限域 + ACL检查
// ============================================================
#pragma once
#include "../types.h"
#include <cstdio>
#include <cstring>
#include <cstdint>

namespace eos {
namespace security {

// ---------- 权限域 ----------
enum PermissionDomain : uint8_t {
    DOMAIN_ROOT   = 0,
    DOMAIN_LOGIC  = 1,
    DOMAIN_USER   = 2,
    DOMAIN_COUNT
};

// ---------- 权限位 ----------
enum PermissionBit : uint64_t {
    PERM_MEM_READ      = 1ULL << 0,
    PERM_MEM_WRITE     = 1ULL << 1,
    PERM_MEM_EXEC      = 1ULL << 2,
    PERM_MEM_MAP_IO    = 1ULL << 3,
    PERM_MEM_LOCK      = 1ULL << 4,
    
    PERM_SYS_SHUTDOWN  = 1ULL << 10,
    PERM_SYS_CREATE_TASK = 1ULL << 11,
    PERM_SYS_KILL_TASK = 1ULL << 12,
    PERM_SYS_SET_PRIO  = 1ULL << 13,
    PERM_SYS_LOAD_DLL  = 1ULL << 14,
    
    PERM_IPC_BROADCAST = 1ULL << 20,
    PERM_IPC_DIRECT    = 1ULL << 21,
    PERM_IPC_LISTEN    = 1ULL << 22,
    
    PERM_AFFECT_READ   = 1ULL << 30,
    PERM_AFFECT_WRITE  = 1ULL << 31,
    PERM_AFFECT_INJECT = 1ULL << 32,
    PERM_MEMORY_ACCESS = 1ULL << 33,
    PERM_MEMORY_SCULPT = 1ULL << 34,
    
    PERM_NEFS_READ     = 1ULL << 40,
    PERM_NEFS_WRITE    = 1ULL << 41,
    PERM_NEFS_FORMAT   = 1ULL << 42,
    
    PERM_ALL           = 0xFFFFFFFFFFFFFFFFULL,
};

inline uint64_t domain_default_permissions(PermissionDomain domain) {
    switch (domain) {
        case DOMAIN_ROOT:
            return 0xFFFFFFFFFFFFFFFFULL;
        case DOMAIN_LOGIC:
            return PERM_MEM_READ | PERM_MEM_WRITE |
                   PERM_IPC_BROADCAST | PERM_IPC_DIRECT | PERM_IPC_LISTEN |
                   PERM_AFFECT_READ | PERM_AFFECT_WRITE |
                   PERM_MEMORY_ACCESS | PERM_MEMORY_SCULPT |
                   PERM_NEFS_READ | PERM_NEFS_WRITE |
                   PERM_SYS_CREATE_TASK;
        case DOMAIN_USER:
        default:
            return PERM_MEM_READ | PERM_MEM_WRITE |
                   PERM_IPC_DIRECT | PERM_IPC_LISTEN |
                   PERM_AFFECT_READ |
                   PERM_NEFS_READ |
                   PERM_SYS_CREATE_TASK;
    }
}

inline bool check_permission(uint64_t task_permissions, PermissionBit required) {
    return (task_permissions & (uint64_t)required) != 0;
}

inline const char* domain_name(PermissionDomain d) {
    switch (d) {
        case DOMAIN_ROOT:  return "ROOT";
        case DOMAIN_LOGIC: return "LOGIC";
        case DOMAIN_USER:  return "USER";
        default:           return "???";
    }
}

} // namespace security
} // namespace eos
