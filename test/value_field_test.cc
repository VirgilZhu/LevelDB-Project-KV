#include "gtest/gtest.h"
#include "leveldb/db.h"
#include "db/fields.h"
#include "leveldb/write_batch.h"

using namespace leveldb;

Status OpenDB(const std::string& dbName, DB** db) {
    // 如果数据库已经存在，则删除它。
    std::string rm_command = "rm -rf " + dbName;
    system(rm_command.c_str());

    Options options;
    options.create_if_missing = true;
    return DB::Open(options, dbName, db);
}

class FieldsTest : public ::testing::Test {
protected:
    void SetUp() override {
        Status s = OpenDB("testdb", &db_);
        EXPECT_TRUE(s.ok()) << "Failed to open database: " << s.ToString();
    }

    void TearDown() override {
        delete db_;
        db_ = nullptr;
    }

    DB* db_ = nullptr; // 数据库实例指针。
};

// 测试批量插入和验证
TEST_F(FieldsTest, TestBulkInsertAndValidation) {
    const size_t num_fields = 1000;
    FieldArray fields;

    // 准备大量字段数据（逆序添加以测试排序）
    for (size_t i = num_fields; i > 0; --i) {
        fields.emplace_back(std::to_string(i), "value_" + std::to_string(i));
    }

    // 创建 Fields 对象并添加所有字段
    Fields f(fields);

    // 验证所有字段都被正确添加
    EXPECT_EQ(f.size(), num_fields); 

    // 排序字段
    f.SortFields();

    // 验证每个字段的存在性和值（现在应该按顺序排列）
    for (size_t i = 1; i <= num_fields; ++i) {
        auto field_name = std::to_string(i);
        EXPECT_TRUE(f.HasField(field_name)) << "Missing field: " << field_name;
        EXPECT_EQ(f.GetField(field_name).second, "value_" + std::to_string(i)) << "Incorrect value for field: " << field_name;
    }

    // 序列化字段，并将结果插入数据库
    std::string serialized_value = f.SerializeValue();
    const std::string key = "bulk_data_key";
    Status status = db_->Put(WriteOptions(), key, serialized_value);
    EXPECT_TRUE(status.ok()) << "Failed to put data into database: " << status.ToString();

    // 从数据库中读取并反序列化字段
    std::string retrieved_value;
    status = db_->Get(ReadOptions(), key, &retrieved_value);
    EXPECT_TRUE(status.ok()) << "Failed to get data from database: " << status.ToString();
    Fields deserialized_fields = Fields::ParseValue(retrieved_value);

    // 验证反序列化的字段是否与原始字段匹配
    EXPECT_EQ(deserialized_fields.size(), num_fields); 
    for (size_t i = 1; i <= num_fields; ++i) {
        auto field_name = std::to_string(i);
        EXPECT_TRUE(deserialized_fields.HasField(field_name)) << "Deserialized field missing: " << field_name;
        EXPECT_EQ(deserialized_fields.GetField(field_name).second, "value_" + std::to_string(i)) << "Incorrect deserialized value for field: " << field_name;
    }

}

// 测试批量删除功能
TEST_F(FieldsTest, TestBulkDelete) {
    const size_t num_fields = 1000;
    FieldArray fields;

    // 准备大量字段数据
    for (size_t i = 0; i < num_fields; ++i) {
        fields.emplace_back(std::to_string(i), "value_" + std::to_string(i));
    }

    // 创建 Fields 对象并添加所有字段
    Fields f(fields);

    // 批量删除一半的字段
    std::vector<std::string> delete_field_names;
    for (size_t i = 0; i < num_fields / 2; ++i) {
        delete_field_names.push_back(std::to_string(i));
    }
    f.DeleteFields(delete_field_names);

    // 验证删除后的字段数量和内容
    EXPECT_EQ(f.size(), num_fields / 2);
    for (size_t i = 0; i < num_fields; ++i) {
        auto field_name = std::to_string(i);
        if (i < num_fields / 2) {
            EXPECT_FALSE(f.HasField(field_name)) << "Deleted field still exists: " << field_name;
        } else {
            EXPECT_TRUE(f.HasField(field_name)) << "Missing non-deleted field: " << field_name;
            EXPECT_EQ(f.GetField(field_name).second, "value_" + std::to_string(i)) << "Incorrect value for non-deleted field: " << field_name;
        }
    }
}

// 测试单个更新操作
TEST_F(FieldsTest, TestSingleUpdate) {
    FieldArray fields = {{"field1", "old_value"}, {"field2", "value2"}};
    Fields f(fields);
    f.UpdateField("field1", "new_value");

    EXPECT_EQ(f.GetField("field1").second, "new_value");
    EXPECT_EQ(f.GetField("field2").second, "value2");

    // 检查排序是否正确
    EXPECT_TRUE(std::is_sorted(f.begin(), f.end(),
                               [](const Field& lhs, const Field& rhs) {
                                   return lhs.first < rhs.first;
                               })) << "Fields are not sorted after single update.";
}

// 测试批量更新操作
TEST_F(FieldsTest, TestBulkUpdate) {
    const size_t num_fields = 500;
    FieldArray fields;

    // 准备大量字段数据
    for (size_t i = 0; i < num_fields; ++i) {
        fields.emplace_back(std::to_string(i), "old_value_" + std::to_string(i));
    }

    // 创建 Fields 对象并添加所有字段
    Fields f(fields);

    // 批量更新一半的字段
    FieldArray update_fields;
    for (size_t i = 0; i < num_fields / 2; ++i) {
        update_fields.emplace_back(std::to_string(i), "new_value_" + std::to_string(i));
    }
    f.UpdateFields(update_fields);

    // 验证更新后的字段值
    for (size_t i = 0; i < num_fields; ++i) {
        auto field_name = std::to_string(i);
        auto expected_value = (i < num_fields / 2) ? ("new_value_" + std::to_string(i)) : ("old_value_" + std::to_string(i));
        EXPECT_EQ(f.GetField(field_name).second, expected_value) << "Incorrect value for updated field: " << field_name;
    }

    // 检查排序是否正确
    EXPECT_TRUE(std::is_sorted(f.begin(), f.end(),
                               [](const Field& lhs, const Field& rhs) {
                                   return lhs.first < rhs.first;
                               })) << "Fields are not sorted after bulk update.";
}

// 测试各种构造函数
TEST_F(FieldsTest, TestConstructors) {
    // 单个 Field 构造
    Fields f_single(Field("single", "value"));
    EXPECT_EQ(f_single.size(), 1);
    EXPECT_TRUE(f_single.HasField("single"));

    // FieldArray 构造
    FieldArray fields = {{"array1", "value1"}, {"array2", "value2"}};
    Fields f_array(fields);
    EXPECT_EQ(f_array.size(), 2);
    EXPECT_TRUE(f_array.HasField("array1"));
    EXPECT_TRUE(f_array.HasField("array2"));

    // field_names 数组构造
    std::vector<std::string> field_names = {"name1", "name2"};
    Fields f_names(field_names);
    EXPECT_EQ(f_names.size(), 2);
}

// 测试 operator[] 访问功能
TEST_F(FieldsTest, TestOperatorBracketAccess) {
    // 创建一个 Fields 对象并添加一些字段
    FieldArray fields = {{"field1", "value1"}, {"field2", "value2"}};
    Fields f(fields);

    // 使用 operator[] 来获取字段值
    EXPECT_EQ(f["field1"], "value1");
    EXPECT_EQ(f["field2"], "value2");

    // 尝试获取不存在的字段，应该返回空字符串并打印错误信息
    testing::internal::CaptureStderr();
    EXPECT_EQ(f["nonexistent_field"], "");
}

// 测试 operator[] 更新功能
TEST_F(FieldsTest, TestOperatorBracketUpdate) {
    // 创建一个 Fields 对象并添加一些字段
    Fields f;

    // 使用 operator[] 来设置字段值（字段不存在时应插入）
    f["field1"] = "value1";
    EXPECT_EQ(f["field1"], "value1");

    // 更新已存在的字段值
    f["field1"] = "new_value1";
    EXPECT_EQ(f["field1"], "new_value1");

    // 插入多个新字段
    f["field2"] = "value2";
    f["field3"] = "value3";

    // 验证所有字段都已正确插入
    EXPECT_EQ(f.size(), 3);
    EXPECT_EQ(f["field1"], "new_value1");
    EXPECT_EQ(f["field2"], "value2");
    EXPECT_EQ(f["field3"], "value3");

    // 检查排序是否正确
    EXPECT_TRUE(std::is_sorted(f.begin(), f.end(),
                               [](const Field& lhs, const Field& rhs) {
                                   return lhs.first < rhs.first;
                               })) << "Fields are not sorted after updates.";
}

// 测试批量插入、排序、序列化/反序列化以及 FindKeysByFields 功能
TEST_F(FieldsTest, TestBulkInsertSortSerializeAndFindKeys) {
    const size_t num_entries = 500;
    leveldb::WriteBatch batch;

    // 准备大量键值对数据（逆序添加以测试排序）
    std::map<std::string, Fields> data_to_insert;
    for (size_t i = num_entries; i > 0; --i) {
        std::string key = "key_" + std::to_string(i);
        FieldArray fields = {{"field1", "value1_"}, {"field2", "value2_"}};
        data_to_insert[key] = Fields(fields);

        Fields ffields = Fields(fields);

        // 将序列化后的字段添加到 WriteBatch 中
        Status status = db_->PutFields(WriteOptions(), Slice(key), ffields);
        EXPECT_TRUE(status.ok()) << "Failed to put fields for key: " << key << ", error: " << status.ToString();
    }

    // 验证插入的数据是否正确
    for (size_t i = 1; i <= num_entries; ++i) {
        std::string key = "key_" + std::to_string(i);
        std::string value;
        Status status = db_->Get(ReadOptions(), key, &value);
        EXPECT_TRUE(status.ok()) << "Failed to read key: " << key << ", error: " << status.ToString();

        // 反序列化并验证字段值
        Fields f = Fields::ParseValue(value);
//        EXPECT_EQ(f["field1"], "value1_" + std::to_string(i)) << "Incorrect value for field1 in key: " << key;
//        EXPECT_EQ(f["field2"], "value2_" + std::to_string(i)) << "Incorrect value for field2 in key: " << key;
    }

    Status status = db_->Delete(WriteOptions(), "key_1");

    // 使用 FindKeysByFields 查找包含特定字段的键
    FieldArray fields_to_find = {{"field2", "value2_"}};
    std::vector<std::string> found_keys = Fields::FindKeysByFields(db_, fields_to_find);

    // 验证找到的键是否正确
    EXPECT_EQ(found_keys.size(), num_entries) << "Expected " << num_entries << " keys but found " << found_keys.size();
    for (size_t i = 1; i <= num_entries; ++i) {
        std::string expected_key = "key_" + std::to_string(i);
        EXPECT_TRUE(std::find(found_keys.begin(), found_keys.end(), expected_key) != found_keys.end())
            << "Key not found: " << expected_key;
    }

    // 再次查找，这次没有符合条件的字段
    FieldArray no_match_fields = {{"nonexistent_field", ""}};
    found_keys = Fields::FindKeysByFields(db_, no_match_fields);
    EXPECT_TRUE(found_keys.empty()) << "Expected an empty result for non-matching fields.";

    
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}