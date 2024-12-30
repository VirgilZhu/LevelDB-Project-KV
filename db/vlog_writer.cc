#include "db/vlog_writer.h"

#include <cstdint>

#include "leveldb/env.h"

#include "util/coding.h"

namespace leveldb {
namespace log {
VlogWriter::VlogWriter(WritableFile* dest) : dest_(dest), head_(0) {}

VlogWriter::VlogWriter(WritableFile* dest, uint64_t dest_length)
    : dest_(dest), head_(0) {}

Status VlogWriter::AddRecord(const Slice& slice, uint64_t& offset) {
  const char* ptr = slice.data();
  size_t left = slice.size();
  Status s;
  s = EmitPhysicalRecord(ptr, left, offset);
  return s;
}

Status VlogWriter::EmitPhysicalRecord(const char* ptr, size_t length,
                                      uint64_t& offset) {
  assert(length <= 0xffff);
  char buf[4];
  EncodeFixed32(buf, length);
  Status s = dest_->Append(Slice(buf, 4));
  if (s.ok()) {
    s = dest_->Append(Slice(ptr, length));
    if (s.ok()) {
      s = dest_->Flush();
      offset = head_ + 4;
      head_ += 4 + length;
    }
  }
  return s;
}

}  // namespace log
}  // namespace leveldb
