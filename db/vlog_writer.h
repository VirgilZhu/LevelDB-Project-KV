#ifndef LEVELDB_VLOG_WRITER_H
#define LEVELDB_VLOG_WRITER_H

#include <map>
#include "leveldb/status.h"

namespace leveldb {

class WritableFile;

namespace vlog {

class VlogInfo;
class VlogManager;

class VlogWriter {
    public:
        explicit VlogWriter(WritableFile* vlog);
        ~VlogWriter();

        /* 往 vlog 的写缓存写入一条 kv 记录 */
        Status AddRecord(const Slice& slice);

    private:
        VlogWriter() = default;

        VlogInfo* vlog_info_;
        WritableFile* vlog_;

        VlogWriter(const VlogWriter&);
        VlogWriter& operator=(const VlogWriter&);

    friend class VlogManager;
};

}  // namespace vlog
}  // namespace leveldb

#endif  // LEVELDB_VLOG_WRITER_H
