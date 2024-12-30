#ifndef LEVELDB_DB_VLOG_WRITER_H_
#define LEVELDB_DB_VLOG_WRITER_H_

#include "db/log_format.h"
#include <cstdint>

#include "leveldb/slice.h"
#include "leveldb/status.h"

namespace leveldb {

class WritableFile;

namespace log {

class VlogWriter {
 public:
  // Create a writer that will append data to "*dest".
  // "*dest" must be initially empty.
  // "*dest" must remain live while this Writer is in use.
  explicit VlogWriter(WritableFile* dest);

  // Create a writer that will append data to "*dest".
  // "*dest" must have initial length "dest_length".
  // "*dest" must remain live while this Writer is in use.
  VlogWriter(WritableFile* dest, uint64_t dest_length);

  VlogWriter(const VlogWriter&) = delete;
  VlogWriter& operator=(const VlogWriter&) = delete;

  ~VlogWriter() = default;

  Status AddRecord(const Slice& slice, uint64_t& offset);

 private:
  Status EmitPhysicalRecord(const char* ptr, size_t length, uint64_t& offset);
  size_t head_;
  WritableFile* dest_;
};

}  // namespace log
}  // namespace leveldb

#endif  // LEVELDB_DB_VLOG_WRITER_H_
