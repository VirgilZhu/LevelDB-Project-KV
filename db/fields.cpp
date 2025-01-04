#include <iostream>
#include <utility>

#include "db/filename.h"

#include "leveldb/env.h"

#include "util/coding.h"

#include "dbformat.h"
#include "fields.h"
#include "vlog_reader.h"
#include "db_impl.h"

namespace leveldb {

/* 构造函数 */
Fields::Fields(FieldArray fields)
    : fields_(std::move(fields)) {
    SortFields();
}

Fields::Fields(const Field& field)
    : fields_({field}) {}

Fields::Fields(const std::vector<std::string>& field_names) {
    for (const auto& name : field_names) {
        fields_.emplace_back(name, "");
    }
    SortFields();
}

/* 根据 field_name 从小到大进行排序，减少通过 field_name 遍历 Fields 的耗时 */
void Fields::SortFields() {
    std::sort(fields_.begin(), fields_.end(), [](const Field& a, const Field& b) {
        return a.first < b.first;
    });
}

/* 更新/插入单个字段值 */
void Fields::UpdateField(const std::string& field_name, const std::string& field_value) {
    for (auto iter = fields_.begin(); iter != fields_.end(); iter++) {
        if ((*iter).first > field_name) {
            fields_.insert(iter, {field_name, field_value});
            return;
        }
        if ((*iter).first == field_name) {
            (*iter).second = field_value;
            return;
        }
    }
    fields_.emplace_back(field_name, field_value);
}

void Fields::UpdateField(const Field& field) {
    UpdateField(field.first, field.second);
}

/* 更新/插入多个字段值 */
void Fields::UpdateFields(const std::vector<std::string>& field_names, const std::vector<std::string>& field_values) {
    if (field_names.size() != field_values.size()) {
        std::cerr << "UpdateFields Failed:  field_name and field_values must have the same size." << std::endl;
        return;
    }
    for (size_t i = 0; i < field_names.size(); ++i) {
        UpdateField(field_names[i], field_values[i]);
    }
}

void Fields::UpdateFields(const FieldArray& fields) {
    for (const auto& field : fields) {
        UpdateField(field);
    }
}

/* 删除单个字段 */
void Fields::DeleteField(const std::string& field_name) {
    for (auto iter = fields_.begin(); iter != fields_.end(); iter++) {
        if ((*iter).first > field_name) return;
        if ((*iter).first == field_name) {
            fields_.erase(iter);
            return;
        }
    }
}

/* 删除多个字段 */
void Fields::DeleteFields(const std::vector<std::string>& field_names) {
    for (auto &name : field_names) {
        if (fields_.empty()) return;
        DeleteField(name);
    }
}

/* 序列化 Field 或 FieldArray 为 value 字符串 */
std::string Fields::SerializeValue(const FieldArray& fields) {
    std::string value_str;
    for (const auto& field : fields) {
        std::string field_str = SerializeValue(field);
        value_str += field_str;
    }
    return value_str;
}

std::string Fields::SerializeValue(const Field& field) {
    std::string value_str;
    PutLengthPrefixedSlice(&value_str, Slice(field.first));
    PutLengthPrefixedSlice(&value_str, Slice(field.second));
    return value_str;
}

std::string Fields::SerializeValue() const {
    return SerializeValue(fields_);
}

/* 反序列化 value 字符串为 Fields */
Fields Fields::ParseValue(const std::string& value_str) {
    Fields fields;
    Slice value_slice(value_str);
    while (!value_slice.empty()) {
        Slice field_name;
        Slice field_value;
        if (!GetLengthPrefixedSlice(&value_slice, &field_name)) break;
        if (!GetLengthPrefixedSlice(&value_slice, &field_value)) break;
        fields.UpdateField(field_name.ToString(), field_value.ToString());
    }
    return fields;
}

/* 获取字段 */
Field Fields::GetField(const std::string& field_name) const {
    for (auto iter = fields_.begin(); iter != fields_.end(); iter++) {
        if ((*iter).first == field_name) return *iter;
        if ((*iter).first > field_name || iter == fields_.end() - 1) {
            std::cerr << "GetField Failed:  field name [" + field_name + "] doesn't exist, return {}." << std::endl;
            return {};
        }
    }
    std::cerr << "GetField Failed:  field name [" + field_name + "] doesn't exist, return {}." << std::endl;
    return {};
}

/* 检查字段是否存在 */
bool Fields::HasField(const std::string& field_name) const {
    for (auto iter = fields_.begin(); iter != fields_.end(); iter++) {
        if ((*iter).first == field_name) return true;
        if ((*iter).first > field_name || iter == fields_.end() - 1) return false;
    }
    return false;
}

/* 重载运算符 [] 用于访问字段值 */
std::string Fields::operator[](const std::string& field_name) const {
    for (auto iter = fields_.begin(); iter != fields_.end(); iter++) {
        if ((*iter).first == field_name) return (*iter).second;
        if ((*iter).first > field_name || iter == fields_.end() - 1) {
            static const std::string empty_str;
            std::cerr << "GetField Failed:  field name [" + field_name + "] doesn't exist." << std::endl;
            return empty_str;
        }
    }
    static const std::string empty_str;
    std::cerr << "GetField Failed:  field name [" + field_name + "] doesn't exist." << std::endl;
    return empty_str;
}

std::string& Fields::operator[](const std::string& field_name) {
    for (auto iter = fields_.begin(); iter != fields_.end(); iter++) {
        if ((*iter).first == field_name) return (*iter).second;
        if ((*iter).first > field_name) {
            return fields_.insert(iter, {field_name, ""})->second;
        }
    }
    fields_.emplace_back(field_name, "");
    return fields_.back().second;
}

/* 通过若干个字段查询 Key */
//std::vector<std::string> Fields::FindKeysByFields(leveldb::DB* db, const FieldArray& fields, const std::string& dbname, Env* env) {
//    Fields to_fields = Fields(fields);
//    to_fields.Fields::SortFields();
//    FieldArray search_fields_ = to_fields.fields_;
//
//    std::vector<std::string> find_keys;
//
//    Iterator* it = db->NewIterator(leveldb::ReadOptions());
//    for (it->SeekToFirst(); it->Valid(); it->Next()) {
//
//        std::string iter_key = it->key().ToString();
//        if (std::find(find_keys.begin(), find_keys.end(), iter_key) != find_keys.end()){
//            continue;
//        }
//
//        FieldArray iter_fields_ = Fields::ParseValue(it->value().ToString()).fields_;
//
//        if (iter_fields_ == search_fields_ ||
//            std::includes(iter_fields_.begin(), iter_fields_.end(),
//                          search_fields_.begin(), search_fields_.end())) {
//            find_keys.emplace_back(iter_key);
//        }
//    }
//
//    assert(it->status().ok());
//    delete it;
//
//    return find_keys;
//}

std::vector<std::string> Fields::FindKeysByFields(leveldb::DB* db, const FieldArray& fields, DBImpl* impl) {
  Fields to_fields = Fields(fields);
  to_fields.Fields::SortFields();
  FieldArray search_fields_ = to_fields.fields_;

  std::vector<std::string> find_keys;

  Iterator* it = db->NewIterator(leveldb::ReadOptions());
  for (it->SeekToFirst(); it->Valid(); it->Next()) {

    std::string iter_key = it->key().ToString();
    if (std::find(find_keys.begin(), find_keys.end(), iter_key) != find_keys.end()){
      continue;
    }

    FieldArray iter_value_ = Fields::ParseValue(it->value().ToString()).fields_;

    if (!iter_value_.empty()) {
      if (iter_value_ == search_fields_ ||
          std::includes(iter_value_.begin(), iter_value_.end(),
                        search_fields_.begin(), search_fields_.end())) {
        find_keys.emplace_back(iter_key);
      }
    } else {
        uint64_t fid;
        uint64_t kv_offset;
        uint64_t val_size;
        Slice vlog_ptr = it->value();
        if (!(GetVarint64(&vlog_ptr, &fid)
            && GetVarint64(&vlog_ptr, &kv_offset)
            && GetVarint64(&vlog_ptr, &val_size)))  {
          continue;
        }
        uint64_t encoded_len = 1 + VarintLength(it->key().size()) + it->key().size() + VarintLength(val_size) + val_size;

        Env* env = impl->GetEnv();
        std::string dbname = impl->GetDBName();

        std::string fname = LogFileName(dbname, fid);
        RandomAccessFile* file;

        Status s = env->NewRandomAccessFile(fname, &file);
        if (!s.ok()) {
          continue;
        }
        struct VlogReporter : public log::VlogReader::Reporter {
          Status* status;
          void Corruption(size_t bytes, const Status& s) override {
            if (this->status->ok()) *this->status = s;
          }
        };
        VlogReporter reporter;
        log::VlogReader vlogReader(file, &reporter);
        Slice key_value;
        std::string vlog_value;
        char* scratch = new char[encoded_len];

        if (vlogReader.ReadValue(kv_offset, encoded_len, &key_value, scratch)) {
          if (!DBImpl::ParseVlogValue(key_value, it->key(), vlog_value, val_size)) {
            s = Status::Corruption("value in vlog isn't match with given key");
          }
        } else {
          s = Status::Corruption("read vlog error");
        }

        delete file;
        file = nullptr;

        iter_value_ = Fields::ParseValue(vlog_value).fields_;
        if (iter_value_ == search_fields_ ||
            std::includes(iter_value_.begin(), iter_value_.end(),
                          search_fields_.begin(), search_fields_.end())) {
          find_keys.emplace_back(iter_key);
        }
    }
  }

//  assert(it->status().ok());
  delete it;

  return find_keys;
}

} // namespace leveldb
