#ifndef LEVELDB_VLOG_READER_H
#define LEVELDB_VLOG_READER_H

#include <cstdint>

#include "db/log_format.h"
#include "leveldb/slice.h"
#include "leveldb/status.h"

namespace leveldb {

class SequentialFile;
class RandomAccessFile;

namespace log {

class VlogReader {
    public:
        class Reporter {
            public:
                virtual ~Reporter() = default;
                virtual void Corruption(size_t bytes, const Status& status) = 0;
        };

        // 支持随机读与顺序读，提升顺序读取的效率
        explicit VlogReader(SequentialFile* file, Reporter* reporter);
        explicit VlogReader(RandomAccessFile* file, Reporter* reporter);

        VlogReader(const VlogReader&) = delete;
        VlogReader& operator=(const VlogReader&) = delete;

        ~VlogReader();
        // 从 vlog 中读取一条具体的值
        bool ReadValue(uint64_t offset, size_t length, Slice* key_value, char* scratch);
        bool ReadRecord(Slice* record, std::string* scratch);
        // 返回 ReadRecord 函数读取的最后一条 record 的偏移（last_record_offset_）
        uint64_t LastRecordOffset() const;

    private:
        /* 读取 vlog 物理记录（ data 部分） */
        bool ReadPhysicalRecord(std::string* result);

        void ReportCorruption(uint64_t bytes, const Status &reason);

        SequentialFile* const file_;
        RandomAccessFile* const file_random_;
        Reporter* const reporter_;
        char* const backing_store_;
        Slice buffer_;
        bool eof_;  // Last Read() indicated EOF by returning < kBlockSize

        // Offset of the last record returned by ReadRecord.
        uint64_t last_record_offset_;
};

}  // namespace log
}  // namespace leveldb

#endif  // LEVELDB_VLOG_READER_H
