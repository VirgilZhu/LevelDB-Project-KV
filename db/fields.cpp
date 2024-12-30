#include <iostream>
#include <utility>

//#include <fstream>

#include "fields.h"
#include "util/coding.h"
#include "dbformat.h"

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
std::vector<std::string> Fields::FindKeysByFields(leveldb::DB* db, const FieldArray& fields) {
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

        FieldArray iter_fields_ = Fields::ParseValue(it->value().ToString()).fields_;

        if (iter_fields_ == search_fields_ ||
            std::includes(iter_fields_.begin(), iter_fields_.end(),
                          search_fields_.begin(), search_fields_.end())) {
            find_keys.emplace_back(iter_key);
        }
    }

    assert(it->status().ok());
    delete it;

    return find_keys;
}

//std::vector<std::string> Fields::FindKeysByFields(leveldb::DB* db, const FieldArray& fields) {
//    Fields to_fields = Fields(fields);
//    to_fields.Fields::SortFields();
//    FieldArray search_fields_ = to_fields.fields_;
//
//    std::vector<std::string> find_keys;
//    std::vector<std::string> deleted_keys;
//
//    std::ofstream outfile;
//    outfile.open("/home/workspace/leveldb_kv/test/log.txt");
//    outfile.clear();
//    outfile << "aaaaaaaaaaaaaaaaaaa\n";
//
//    Iterator* it = db->NewIterator(leveldb::ReadOptions());
//    for (it->SeekToFirst(); it->Valid(); it->Next()) {
//
//        Slice iter_key_slice = it->key();
//        outfile << "\niter_key_slice: " + iter_key_slice.ToString() + "\n";
//        const char* p = iter_key_slice.data();
//        const char* limit = p + iter_key_slice.size();
////        const char* limit = p + 5;
//
//        uint32_t key_length;
//        const char* key_ptr = GetVarint32Ptr(p, limit, &key_length);
//
//        iter_key_slice = Slice(p, iter_key_slice.size() - 8);
//
////        outfile << "\niter_key_slice: " + iter_key_slice.ToString() + "\n";
//
//        Slice iter_key_for_parse;
//        if (!GetLengthPrefixedSlice(&iter_key_slice, &iter_key_for_parse)) {
//            continue;
//        }
//
//        std::string iter_key = iter_key_for_parse.ToString();
//
//        outfile << "\niter_key_str: " + iter_key + "\n";
//
//        if (std::find(deleted_keys.begin(), deleted_keys.end(), iter_key) != deleted_keys.end()
//            || std::find(find_keys.begin(), find_keys.end(), iter_key) != find_keys.end()) {
//            continue;
//        }
//
//        const uint64_t tag = DecodeFixed64(key_ptr + key_length - 8);
//        if ((tag & 0xff) == kTypeDeletion) {
//            deleted_keys.emplace_back(iter_key);
//            continue;
//        }
//
//        outfile << "\niter_tag_str: " + std::to_string(tag) + "\n";
//
//        Slice iter_value_slice = it->value();
//        Slice iter_value_for_parse;
//        if (!GetLengthPrefixedSlice(&iter_value_slice, &iter_value_for_parse)) {
//            continue;
//        }
//
//        outfile << "\niter_value_str: " + iter_value_for_parse.ToString() + "\n";
//
//        FieldArray iter_fields_ = Fields::ParseValue(iter_value_for_parse.ToString()).fields_;
//        // FieldArray iter_fields_ = Fields::ParseValue(it->value().ToString()).fields_;
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

} // namespace leveldb
