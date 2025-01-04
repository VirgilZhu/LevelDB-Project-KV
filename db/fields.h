#ifndef LEVELDB_FIELDS_H
#define LEVELDB_FIELDS_H

#include "leveldb/db.h"

#include "db_impl.h"
#include "vector"

namespace leveldb {

using Field = std::pair<std::string, std::string>;
using FieldArray = std::vector<Field>;

class Fields {
    private:
        FieldArray fields_;

    public:
        /* 从 FieldArray 构造 */
        explicit Fields(FieldArray fields);
        /* 从单个 Field 构造 */
        explicit Fields(const Field& field);
        /* 只传参 field_name 数组的构造 */
        explicit Fields(const std::vector<std::string>& field_names);

        Fields() = default;
        ~Fields() = default;

        /* 根据 field_name 从小到大进行排序，减少通过 field_name 遍历 Fields 的耗时 */
        void SortFields();

        /* 更新/插入单个字段值，插入后会进行 field_name 的排序 */
        void UpdateField(const std::string& field_name, const std::string& field_value);
        void UpdateField(const Field& field);
        /* 更新/插入多个字段值 */
        void UpdateFields(const std::vector<std::string>& field_names, const std::vector<std::string>& field_values);
        void UpdateFields(const FieldArray& fields);

        /* 删除单个字段 */
        void DeleteField(const std::string& field_name);
        /* 删除多个字段 */
        void DeleteFields(const std::vector<std::string>& field_names);

        /* 序列化 Field 或 FieldArray 为 value 字符串 */
        static std::string SerializeValue(const FieldArray& fields);
        static std::string SerializeValue(const Field& field);
        std::string SerializeValue() const;

        /* 反序列化 value 字符串为 Fields */
        static Fields ParseValue(const std::string& value_str);

        /* 获取字段 */
        Field GetField(const std::string& field_name) const;
        /* 检查字段是否存在 */
        bool HasField(const std::string& field_name) const;

        /* 重载运算符 [] 用于访问字段值 */
        std::string operator[](const std::string& field_name) const;
        /* 重载运算符 [] 用于修改字段值 */
        std::string& operator[](const std::string& field_name);

        /* 通过若干个字段查询 Key */
        static std::vector<std::string> FindKeysByFields(leveldb::DB* db, const FieldArray& fields, leveldb::DBImpl* impl);

        using iterator = std::vector<Field>::iterator;
        using const_iterator = std::vector<Field>::const_iterator;

        iterator begin() { return fields_.begin(); }
        const_iterator begin() const { return fields_.cbegin(); }
        iterator end() { return fields_.end(); }
        const_iterator end() const { return fields_.cend(); }
        size_t size() const { return fields_.size(); }
};

}  // namespace leveldb

#endif  // LEVELDB_FIELDS_H
