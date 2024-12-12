#ifndef LEVELDB_VLOG_MANAGER_H
#define LEVELDB_VLOG_MANAGER_H

#include <map>
#include <set>
#include <cstdint>
#include "leveldb/env.h"

namespace leveldb {
namespace vlog {

class VlogWriter;
class VlogReader;

class VlogInfo {
    private:
        /* vlog 当前大小 */
        size_t size_;
        /* vlog 当前若插入新 record 的偏移 */
        size_t head_;
        /* vlog 当前过期的 record 数量 */
        uint64_t expired_count_;

        VlogReader* vlog_read_;
        VlogWriter* vlog_write_;

    public:
        VlogInfo() : size_(0), head_(0), expired_count_(0) {}
        ~VlogInfo() = default;

    friend class VlogWriter;
    friend class VlogReader;
    friend class VlogManager;
};

class VlogManager {
    public:
        explicit VlogManager(uint64_t expired_threshold);
        ~VlogManager() = default;

        /* 新建一个 vlog 并编号 */
        void AddVlog(uint64_t vlog_no);
        /* 往当前活跃 vlog 插入一条 record */
        Status AddRecord(const Slice& slice);
        /* 更新当前活跃 vlog 的头部位置 */
        Status SetHead(size_t offset);
        /* 当前活跃 vlog 的写缓冲的数据全部写入磁盘 */
        Status Sync();
        /* 从某个 vlog 中获取 value */
        Status FetchValueFromVlog(Slice record_addr, std::string* value);
        /* 设置当前活跃 vlog */
        void SetCurrentVlog(uint64_t vlog_no);

    private:
        /* 管理所有 vlog 的编号和信息 */
        std::map<uint64_t, VlogInfo*> vlog_manager_;
        /* 记录正在 GC 的 vlog 集合 */
        std::set<uint64_t> gc_vlog_set_;
        /* 记录触发 GC 的过期阈值 */
        uint64_t expired_threshold_;
        /* 记录当前活跃的 vlog 编号 */
        uint64_t cur_vlog_;
};

} // namespace vlog
} // namespace leveldb

#endif  // LEVELDB_VLOG_MANAGER_H
