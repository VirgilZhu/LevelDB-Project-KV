#ifndef LEVELDB_KV_SEPARATE_MANAGEMENT_H
#define LEVELDB_KV_SEPARATE_MANAGEMENT_H

#include <unordered_map>
#include <unordered_set>

#include <deque>
#include "leveldb/slice.h"
#include <iterator>


namespace leveldb {

typedef struct ValueLogInfo {
    uint64_t last_sequence_;            // VLog 中最后一个有效 kv 的序列号
    size_t file_size_;                  // VLog 文件大小
    uint64_t logfile_number_;           // VLog 文件编号
    int left_kv_numbers_;               // VLog 中剩下的 kv 数量
    uint64_t invalid_memory_;           // VLog 中无效空间的大小
}ValueLogInfo;

struct MapCmp{
    bool operator ()(const ValueLogInfo* a, const ValueLogInfo* b)
    {
        return a->invalid_memory_ < b->invalid_memory_;
    }
};

class SeparateManagement {
 public:
  SeparateManagement(uint64_t garbage_collection_threshold)
      : garbage_collection_threshold_(garbage_collection_threshold) {}

  ~SeparateManagement() {}
  /* 更新数据库的最后一个序列号，为需要 GC 的 VLog 分配新的序列号，返回是否触发 GC */
  bool ConvertQueue(uint64_t& db_sequence);
  /* 更新指定 VLogInfo 的无效空间和剩余键值对数量 */
  void UpdateMap(uint64_t fid, uint64_t abandon_memory);
  /* 选择无效空间最大的 VLog 加入到 need_updates_ 队列 */
  void UpdateQueue(uint64_t fid);
  /* 从 garbage_collection_ 队列中取出一个需要 GC 的 VLog，返回最后（最新）序列号 */
  bool GetGarbageCollectionQueue(uint64_t& fid, uint64_t& last_sequence);
  /* 在 map_file_info_ 中添加新创建的 VLog 的 ValueLogInfo */
  void WriteFileMap(uint64_t fid, int kv_numbers, size_t log_memory);
  /* 检查是否有任何 VLog 需要 GC */
  bool MayNeedGarbageCollection() { return !garbage_collection_.empty(); }
  /* 从 map_file_info_ 中移除指定编号的 VLog */
  void RemoveFileFromMap(uint64_t fid) { map_file_info_.erase(fid); }
  /* 检查 map_file_info_ 是否为空 */
  bool EmptyMap() { return map_file_info_.empty(); }
  /* 遍历 map_file_info_ 中的所有 VLog，更新 need_updates_ 以便后续 GC */
  void CollectionMap();

 private:
  // VLog 触发 GC 的阈值
  uint64_t garbage_collection_threshold_;
  // 当前 version 的所有 VLog 索引
  std::unordered_map<uint64_t, ValueLogInfo*> map_file_info_;
  // 即将 GC 的 VLog
  std::deque<ValueLogInfo*> garbage_collection_;
  // 需要 GC 但尚未更新 Sequence 的 VLog
  std::deque<ValueLogInfo*> need_updates_;
  // 正在删除的 VLog 文件编号
  std::unordered_set<uint64_t> delete_files_;
};

} // namespace leveldb

#endif //LEVELDB_KV_SEPARATE_MANAGEMENT_H