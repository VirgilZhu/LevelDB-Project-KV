#include "vlog_reader.h"
#include "leveldb/env.h"
#include "util/coding.h"

namespace leveldb {

namespace log {

VlogReader::VlogReader(SequentialFile *file, Reporter* reporter)
    : file_(file),
      file_random_(nullptr),
      reporter_(reporter),
      backing_store_(new char[kBlockSize]),
      buffer_(),
      eof_(false),
      last_record_offset_(0) {}

VlogReader::VlogReader(RandomAccessFile *file, Reporter* reporter)
    : file_(nullptr),
      file_random_(file),
      reporter_(reporter),
      backing_store_(new char[kBlockSize]),
      buffer_(),
      eof_(false),
      last_record_offset_(0) {}

VlogReader::~VlogReader() { delete[] backing_store_; }

bool VlogReader::ReadValue(uint64_t offset, size_t length, Slice *key_value, char *scratch) {
  if (file_random_ == nullptr) {
    return false;
  }
  Status status = file_random_->Read(offset, length, key_value, scratch);
  if (!status.ok()) {
    return false;
  }
  return true;
}

bool VlogReader::ReadRecord(Slice *record, std::string *scratch) {
  if (ReadPhysicalRecord(scratch)) {
    *record = *scratch;
    return true;
  }
  return false;
}

uint64_t VlogReader::LastRecordOffset() const {
  return last_record_offset_;
}

void VlogReader::ReportCorruption(uint64_t bytes, const Status &reason) {
  if (reporter_ != nullptr) {
    reporter_->Corruption(static_cast<size_t>(bytes), reason);
  }
}

bool VlogReader::ReadPhysicalRecord(std::string *result) {
  result->clear();
  buffer_.clear();

  char* tmp_head = new char[vHeaderSize];
  Status status = file_->Read(vHeaderSize, &buffer_, tmp_head);
  if (!status.ok()) {
    buffer_.clear();
    ReportCorruption(kBlockSize, status);
    eof_ = true;
    return false;
  } else if (buffer_.size() < vHeaderSize) {
    eof_ = true;
  }

  if (!eof_) {
    result->assign(buffer_.data(),buffer_.size());
    uint32_t length = DecodeFixed32(buffer_.data());
    buffer_.clear();
    char* tmp = new char[length];
    status = file_->Read(length, &buffer_, tmp);
    if (status.ok() && buffer_.size() == length) {
      *result += buffer_.ToString();
    } else {
      eof_ = true;
    }
    delete [] tmp;
  }
  delete [] tmp_head;

  if (eof_) {
    result->clear();
    return false;
  }
  return true;
}

}  // namespace log
}  // namespace leveldb
