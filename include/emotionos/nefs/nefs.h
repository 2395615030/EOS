// EmotionOS - NEFS v1.0: Real Persistent Filesystem
#pragma once
#include "../types.h"
#include <cstdio>
#include <cstring>
#include <cstdint>
#include <cmath>
#include <vector>
#include <string>
#include <algorithm>

namespace eos { namespace nefs {

constexpr uint32_t NEFS_MAGIC        = 0x4E454653;
constexpr uint32_t NEFS_VERSION      = 0x00010000;
constexpr uint32_t SUPERBLOCK_SIZE   = 64;
constexpr uint32_t INODE_TABLE_SIZE  = 256;
constexpr uint32_t INODE_SIZE        = 128;
constexpr uint32_t CHUNK_POOL_SIZE   = 4096;
constexpr uint32_t CHUNK_SIZE        = 4096;
constexpr uint32_t SAL_MAX_ENTRIES   = 4096;
constexpr uint64_t NEFS_TOTAL_SIZE   = 64ULL + (INODE_TABLE_SIZE*INODE_SIZE) + (CHUNK_POOL_SIZE*CHUNK_SIZE);
constexpr uint32_t NEFS_PATH_MAX     = 256;
constexpr uint32_t NAME_MAX          = 64;
constexpr uint32_t MAX_DST_SHAPE     = 8;
constexpr uint32_t MAX_DST_ENTRIES   = 65536;
constexpr uint32_t MAX_CHUNKS_PER_DST = 64;

#pragma pack(push, 1)
struct SuperBlock {
  uint32_t magic; uint32_t version; uint32_t flags;
  uint32_t inode_count; uint32_t next_inode_id;
  uint32_t chunk_count; uint32_t sal_entries;
  uint32_t last_sal_commit; uint32_t checksum; uint32_t reserved[9];
};
struct Inode {
  uint32_t id; uint32_t parent_id; uint32_t type;
  char name[NAME_MAX]; uint32_t ndim;
  int64_t shape[MAX_DST_SHAPE];
  uint32_t entry_count;
  uint32_t chunk_ids[MAX_CHUNKS_PER_DST]; uint32_t chunk_count;
  float emotion_profile[8]; float hormone_profile[16];
  uint32_t flags; uint32_t checksum;
};
struct ChunkHeader {
  uint32_t inode_id; uint32_t data_size; uint32_t checksum; float emotion_tag;
};
struct SALEntry {
  uint64_t seq; uint32_t op; uint32_t target; uint32_t dlen; uint32_t csum; uint8_t data[48];
};
#pragma pack(pop)
// ---- StatInfo: returned by stat_info() ----
struct StatInfo {
  uint32_t id;
  uint32_t parent_id;
  uint32_t type;
  char name[64];
  uint32_t ndim;
  int64_t shape[8];
  uint32_t entry_count;
  uint32_t chunk_count;
  float emotion_profile[8];
  float hormone_profile[16];
  uint32_t flags;
};

class NEFS {
public:
  NEFS() : inodes_(nullptr), chunks_(nullptr), sal_(nullptr),
           n_inodes_(0), n_chunks_(0), n_sal_(0), dirty_(false), mounted_(false) {}
  ~NEFS() { if (mounted_) umount(); delete[] inodes_; delete[] chunks_; delete[] sal_; }

  bool format(const char* storage_path) {
    if (storage_path) storage_path_ = storage_path; else storage_path_.clear();
    inodes_ = new Inode[INODE_TABLE_SIZE]();
    chunks_ = new uint8_t[CHUNK_POOL_SIZE * CHUNK_SIZE]();
    sal_ = new SALEntry[SAL_MAX_ENTRIES]();
    memset(&sb_, 0, sizeof(sb_));
    sb_.magic = NEFS_MAGIC; sb_.version = NEFS_VERSION;
    sb_.inode_count = 1; sb_.next_inode_id = 2;
    sb_.chunk_count = 0; sb_.sal_entries = 0;
    auto& root = inodes_[0]; root.id = 1; root.parent_id = 1; root.type = 1;
    strncpy(root.name, "/", NAME_MAX-1);
    n_inodes_ = 1; n_chunks_ = 0; n_sal_ = 0;
    dirty_ = true; mounted_ = true;
    printf("[NEFS] Format OK. path=%s\n", storage_path ? storage_path : "(mem)");
    return true;
  }
  bool mount(const char* img_path) {
    if (!img_path) return format(nullptr);
    storage_path_ = img_path;
    FILE* f = fopen(img_path, "rb");
    if (!f) { printf("[NEFS] mount: new image %s\n", img_path); return format(img_path); }
    inodes_ = new Inode[INODE_TABLE_SIZE]();
    chunks_ = new uint8_t[CHUNK_POOL_SIZE * CHUNK_SIZE]();
    sal_ = new SALEntry[SAL_MAX_ENTRIES]();
    fread(&sb_, 1, 64, f);
    if (sb_.magic != NEFS_MAGIC) { fclose(f); return format(img_path); }
    fread(inodes_, INODE_SIZE, INODE_TABLE_SIZE, f); n_inodes_ = sb_.inode_count;
    fread(chunks_, CHUNK_SIZE, CHUNK_POOL_SIZE, f); n_chunks_ = sb_.chunk_count;
    fread(sal_, sizeof(SALEntry), SAL_MAX_ENTRIES, f); n_sal_ = sb_.sal_entries;
    fclose(f); dirty_ = false; mounted_ = true;
    printf("[NEFS] Mount: %s | inodes=%d chunks=%d\n", img_path, n_inodes_, n_chunks_);
    return true;
  }
  bool umount() {
    if (!mounted_) return true;
    if (dirty_ && !storage_path_.empty()) {
      sb_.inode_count = n_inodes_; sb_.chunk_count = n_chunks_; sb_.sal_entries = n_sal_;
      FILE* f = fopen(storage_path_.c_str(), "wb");
      if (f) {
        fwrite(&sb_, 1, 64, f);
        fwrite(inodes_, INODE_SIZE, INODE_TABLE_SIZE, f);
        fwrite(chunks_, CHUNK_SIZE, CHUNK_POOL_SIZE, f);
        fwrite(sal_, sizeof(SALEntry), SAL_MAX_ENTRIES, f);
        fclose(f);
        printf("[NEFS] Umount: %s\n", storage_path_.c_str());
      } else printf("[NEFS] Umount FAILED: %s\n", storage_path_.c_str());
    }
    mounted_ = false; dirty_ = false;
    return true;
  }
  uint32_t resolve(const char* path_str) {
    if (!path_str || path_str[0] != '/' || !mounted_) return 0;
    if (strcmp(path_str, "/") == 0) return 1;
    char buf[NEFS_PATH_MAX]; strncpy(buf, path_str, NEFS_PATH_MAX-1);
    uint32_t cur = 1;
    char* tok = strtok(buf, "/");
    while (tok) {
      uint32_t next = 0;
      for (uint32_t i = 0; i < n_inodes_; i++)
        if (inodes_[i].parent_id == cur && strcmp(inodes_[i].name, tok) == 0)
          { next = inodes_[i].id; break; }
      if (!next) return 0;
      cur = next; tok = strtok(nullptr, "/");
    }
    return cur;
  }
  uint32_t mkdir(const char* path_str) {
    if (!mounted_ || n_inodes_ >= INODE_TABLE_SIZE-1) return 0;
    char buf[NEFS_PATH_MAX]; strncpy(buf, path_str, NEFS_PATH_MAX-1);
    char* ls = strrchr(buf, '/'); if (!ls) return 0;
    *ls = 0; uint32_t parent = (strlen(buf)==0) ? 1 : resolve(buf); *ls = '/';
    if (!parent) return 0;
    const char* name = ls + 1;
    for (uint32_t i = 0; i < n_inodes_; i++)
      if (inodes_[i].parent_id == parent && strcmp(inodes_[i].name, name) == 0) return 0;
    auto& in = inodes_[n_inodes_++];
    in.id = n_inodes_; in.parent_id = parent; in.type = 1;
    strncpy(in.name, name, NAME_MAX-1);
    dirty_ = true; return in.id;
  }
  uint32_t create(const char* name, uint32_t parent, const int64_t* shape, uint32_t ndim) {
    if (!mounted_ || n_inodes_ >= INODE_TABLE_SIZE-1 || ndim > MAX_DST_SHAPE) return 0;
    for (uint32_t i = 0; i < n_inodes_; i++)
      if (inodes_[i].parent_id == parent && strcmp(inodes_[i].name, name) == 0) return 0;
    auto& in = inodes_[n_inodes_++];
    in.id = n_inodes_; in.parent_id = parent; in.type = 0;
    strncpy(in.name, name, NAME_MAX-1); in.ndim = ndim;
    in.entry_count = 0; in.chunk_count = 0;
    for (uint32_t i = 0; i < ndim; i++) in.shape[i] = shape[i];
    memset(in.chunk_ids, 0, sizeof(in.chunk_ids));
    memset(in.emotion_profile, 0, 32);
    memset(in.hormone_profile, 0, 64);
    dirty_ = true; return in.id;
  }
  uint32_t create_at(const char* path_str, const int64_t* shape, uint32_t ndim) {
    char buf[NEFS_PATH_MAX]; strncpy(buf, path_str, NEFS_PATH_MAX-1);
    char* ls = strrchr(buf, '/');
    if (!ls) return 0;
    *ls = 0; uint32_t parent = (strlen(buf) == 0) ? 1 : resolve(buf);
    *ls = '/';
    if (!parent) return 0;
    return create(ls+1, parent, shape, ndim);
  }
  struct Entry { int64_t idx[8]; float value; float emotion_tag; };
  bool write_entry(uint32_t ino, const int64_t* idx, float val, float etag) {
    auto* in = get_inode(ino); if (!in || in->type != 0) return false;
    uint8_t buf[16]; int64_t i0 = idx ? idx[0] : 0;
    memcpy(buf, &i0, 8); memcpy(buf+8, &val, 4); memcpy(buf+12, &etag, 4);
    uint32_t cid = 0;
    if (in->chunk_count > 0) {
      cid = in->chunk_ids[in->chunk_count-1];
      auto* h = chunk_hdr(cid);
      if (h->data_size + 16 > CHUNK_SIZE - (uint32_t)sizeof(ChunkHeader)) cid = 0;
    }
    if (cid == 0) {
      cid = alloc_chunk(); if (cid >= CHUNK_POOL_SIZE || in->chunk_count >= MAX_CHUNKS_PER_DST) return false;
      in->chunk_ids[in->chunk_count++] = cid;
    }
    append_chunk(cid, buf, 16, ino, etag);
    in->entry_count++; dirty_ = true; return true;
  }
  std::vector<Entry> read_entries(uint32_t ino) {
    std::vector<Entry> r; auto* in = get_inode(ino); if (!in || in->type) return r;
    for (uint32_t ci = 0; ci < in->chunk_count; ci++) {
      uint32_t cid = in->chunk_ids[ci]; if (cid >= n_chunks_) continue;
      auto* h = chunk_hdr(cid); if (h->inode_id != ino || !h->data_size) continue;
      uint8_t* d = chunks_ + cid * CHUNK_SIZE + sizeof(ChunkHeader);
      uint32_t pos = 0;
      while (pos + 16 <= h->data_size) {
        Entry e; memset(e.idx, 0, sizeof(e.idx));
        memcpy(e.idx, d+pos, 8); memcpy(&e.value, d+pos+8, 4);
        memcpy(&e.emotion_tag, d+pos+12, 4);
        r.push_back(e); pos += 16;
      }
    }
    return r;
  }
  bool set_emotion(uint32_t ino, const float* e, const float* h) {
    auto* in = get_inode(ino); if (!in) return false;
    memcpy(in->emotion_profile, e, 32); if (h) memcpy(in->hormone_profile, h, 64);
    dirty_ = true; return true;
  }
  std::vector<uint32_t> find_by_emotion(const float* q, float th) {
    std::vector<uint32_t> r;
    for (uint32_t i = 0; i < n_inodes_; i++) {
      if (inodes_[i].type) continue; float s = 0;
      for (int e = 0; e < 8; e++) s += inodes_[i].emotion_profile[e] * q[e];
      if (s > th) r.push_back(inodes_[i].id);
    }
    return r;
  }
  Inode* get_inode(uint32_t id) {
    for (uint32_t i = 0; i < n_inodes_; i++) if (inodes_[i].id == id) return &inodes_[i];
    return nullptr;
  }
  uint32_t inode_count() const { return n_inodes_; }
  uint32_t tensor_count() const { return n_inodes_; }
  uint32_t chunk_count() const { return n_chunks_; }
  bool is_mounted() const { return mounted_; }
  bool remove(const char* path_str) {
    if (!mounted_) return false;
    uint32_t id = resolve(path_str);
    if (!id || id == 1) return false;
    int idx = -1;
    for (uint32_t i = 0; i < n_inodes_; i++) if (inodes_[i].id == id) { idx = (int)i; break; }
    if (idx < 0) return false;
    if (inodes_[idx].type == 1)
      for (uint32_t i = 0; i < n_inodes_; i++)
        if (inodes_[i].parent_id == id && i != (uint32_t)idx) return false;
    for (uint32_t ci = 0; ci < inodes_[idx].chunk_count; ci++) {
      uint32_t cid = inodes_[idx].chunk_ids[ci];
      if (cid < n_chunks_) { auto* h = chunk_hdr(cid); h->inode_id = 0; h->data_size = 0; } }
    for (uint32_t i = (uint32_t)idx; i < n_inodes_ - 1; i++) inodes_[i] = inodes_[i + 1];
    n_inodes_--;
    for (uint32_t i = 0; i < n_inodes_; i++)
      if (inodes_[i].parent_id == id) inodes_[i].parent_id = 1;
    dirty_ = true; return true;
  }
  bool rename(const char* old_path, const char* new_name) {
    if (!mounted_) return false;
    uint32_t id = resolve(old_path); if (!id) return false;
    auto* in = get_inode(id); if (!in) return false;
    if (strlen(new_name) >= 64) return false;
    strncpy(in->name, new_name, 63); in->name[63] = 0;
    const char* last_slash = strrchr(new_name, '/');
    if (last_slash) {
      char parent_path[256]; size_t plen = (size_t)(last_slash - new_name);
      if (plen > 0 && plen < 255) {
        memcpy(parent_path, new_name, plen); parent_path[plen] = 0;
        uint32_t new_parent = resolve(parent_path);
        if (new_parent && new_parent != id) in->parent_id = new_parent;
      }
      memmove(in->name, last_slash + 1, strlen(last_slash + 1) + 1);
    }
    dirty_ = true; return true;
  }
  StatInfo stat_info(const char* path_str) {
    StatInfo si; memset(&si, 0, sizeof(si));
    uint32_t id = resolve(path_str); if (!id) return si;
    auto* in = get_inode(id); if (!in) return si;
    si.id = in->id; si.parent_id = in->parent_id; si.type = in->type;
    strncpy(si.name, in->name, 63); si.ndim = in->ndim;
    si.entry_count = in->entry_count; si.chunk_count = in->chunk_count;
    memcpy(si.shape, in->shape, sizeof(int64_t) * 8);
    memcpy(si.emotion_profile, in->emotion_profile, 32);
    memcpy(si.hormone_profile, in->hormone_profile, 64);
    si.flags = in->flags; return si;
  }
  bool chmod(uint32_t ino, uint32_t new_flags) {
    auto* in = get_inode(ino); if (!in) return false;
    in->flags = new_flags; dirty_ = true; return true;
  }
  uint64_t disk_used() const { return 64ULL + (n_inodes_ * (uint64_t)INODE_SIZE) + (n_chunks_ * (uint64_t)CHUNK_SIZE); }
  uint64_t disk_capacity() const { return NEFS_TOTAL_SIZE; }
  float disk_usage_pct() const { return (float)disk_used() / (float)NEFS_TOTAL_SIZE * 100.0f; }

  void print_status() const {
    printf("[NEFS] inodes=%d chunks=%d/%d dirty=%s path=%s\n",
           n_inodes_, n_chunks_, CHUNK_POOL_SIZE,
           dirty_ ? "Y" : "N", storage_path_.c_str());
  }
  void print_tree() const {
    printf("[NEFS] Tree:\n");
    for (uint32_t i = 0; i < n_inodes_; i++)
      if (inodes_[i].id == 1) { print_rec(i, 0); break; }
  }
  void print_rec(uint32_t idx, int depth) const {
    for (int d = 0; d < depth; d++) printf("  ");
    if (inodes_[idx].type == 1) printf("[%s/]\n", inodes_[idx].name);
    else printf("  %s (id=%d entries=%d)\n", inodes_[idx].name, inodes_[idx].id, inodes_[idx].entry_count);
    for (uint32_t i = 0; i < n_inodes_; i++)
      if (inodes_[i].id != inodes_[idx].id && inodes_[i].parent_id == inodes_[idx].id) print_rec(i, depth+1);
  }

private:
  SuperBlock sb_;
  Inode* inodes_; uint8_t* chunks_; SALEntry* sal_;
  uint32_t n_inodes_, n_chunks_, n_sal_; bool dirty_, mounted_;
  std::string storage_path_;
  ChunkHeader* chunk_hdr(uint32_t cid) { return (ChunkHeader*)(chunks_ + cid * CHUNK_SIZE); }
  uint32_t alloc_chunk() {
    if (n_chunks_ >= CHUNK_POOL_SIZE) return 0;
    uint32_t c = n_chunks_++; auto* h = chunk_hdr(c);
    h->inode_id = 0; h->data_size = 0; h->checksum = 0; h->emotion_tag = 0; return c;
  }
  void append_chunk(uint32_t cid, const uint8_t* data, uint32_t len, uint32_t ino, float etag) {
    auto* h = chunk_hdr(cid);
    if (!h->data_size) { h->inode_id = ino; h->emotion_tag = etag; }
    uint32_t off = h->data_size;
    uint32_t avail = CHUNK_SIZE - (uint32_t)sizeof(ChunkHeader) - off;
    uint32_t cp = (len < avail) ? len : avail;
    memcpy(chunks_ + cid * CHUNK_SIZE + sizeof(ChunkHeader) + off, data, cp);
    h->data_size += cp;
  }
};

}} // namespace