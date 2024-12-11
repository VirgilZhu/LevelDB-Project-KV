#ifndef LEVELDB_VLOG_READER_H
#define LEVELDB_VLOG_READER_H

#include <cstdint>
#include "leveldb/slice.h"
#include "leveldb/status.h"

namespace leveldb {

class RandomAccessFile;

namespace vlog {

class VlogInfo;
class VlogManager;

class VlogReader {
    public:
        VlogReader(uint32_t vlog_no);
        ~VlogReader() = default;

        /* 在 vlog_offset 处往后读取一条 record 的 value */
        Status Get(uint64_t vlog_offset, std::string* value);

    private:
        VlogInfo* vlog_info_;
        RandomAccessFile* vlog_;

    friend class VlogManager;
};

} // namespace vlog
} // namespace leveldb

#endif  // LEVELDB_VLOG_READER_H
