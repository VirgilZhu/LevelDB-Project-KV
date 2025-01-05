#include "gtest/gtest.h"
#include "leveldb/db.h"
#include "db/fields.h"
#include "leveldb/write_batch.h"

using namespace leveldb;

Status OpenDB(const std::string& dbName, DB** db) {
  // 如果数据库已经存在，则删除它。
//   std::string rm_command = "rm -rf " + dbName;
//   system(rm_command.c_str());

  Options options;
  options.create_if_missing = true;
  return DB::Open(options, dbName, db);
}

class FieldsTest : public ::testing::Test {
 protected:
  void SetUp() override {
    Status s = OpenDB(dbname_ , &db_);
    EXPECT_TRUE(s.ok()) << "Failed to open database: " << s.ToString();
  }

  void TearDown() override {
    delete db_;
    db_ = nullptr;
  }

  DB* db_ = nullptr; // 数据库实例指针。
  std::string dbname_ = "testdb_field"; // 记录数据库路径
};

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

// 测试构造函数内的SortFields的实现
TEST_F(FieldsTest, TestSortFields) {
  // 准备一组未排序的字段数据
  FieldArray unsorted_fields = {
      {"field3", "value3"},
      {"field1", "value1"},
      {"field2", "value2"},
      {"field5", "value5"},
      {"field4", "value4"}
  };

  // 创建 Fields 对象，构造函数应该自动调用 SortFields
  Fields f(unsorted_fields);

  // 验证字段是否已经正确排序
  EXPECT_TRUE(std::is_sorted(f.begin(), f.end(),
                             [](const Field& lhs, const Field& rhs) {
                               return lhs.first < rhs.first;
                             })) << "Fields are not sorted after constructor.";

  // 验证排序后的字段顺序是否符合预期
  std::vector<std::string> expected_order = {"field1", "field2", "field3", "field4", "field5"};
  size_t index = 0;
  for (const auto& field : f) {
    EXPECT_EQ(field.first, expected_order[index++]) << "Field order is incorrect after constructor sorting.";
  }
}

// 测试 operator[] 访问功能
TEST_F(FieldsTest, TestOperatorBracketAccess) {
  // 创建一个 Fields 对象并添加一些字段
  FieldArray fields = {{"field1", "value1"}, {"field2", "value2"}};
  Fields f(fields);

  // 使用 operator[] 来获取字段值
  EXPECT_EQ(f["field1"], "value1");
  EXPECT_EQ(f["field2"], "value2");

  // 尝试获取不存在的字段，应该返回空字符串
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

}

// 测试批量删除功能
TEST_F(FieldsTest, TestBulkDelete) {
  const size_t num_fields = 1000;
  leveldb::WriteBatch batch;

  // 准备大量字段数据，并通过 PutFields 插入到数据库
  for (size_t i = 0; i < num_fields; ++i) {
    std::string key = "key_" + std::to_string(i);
    FieldArray fields = {{"field" + std::to_string(i), "value_" + std::to_string(i)}};
    Fields f(fields);
    Status status = db_->PutFields(WriteOptions(), Slice(key), f);
    EXPECT_TRUE(status.ok()) << "Failed to put fields for key: " << key;
  }

  // 批量删除一半的字段
  for (size_t i = 0; i < num_fields / 2; ++i) {
    std::string key = "key_" + std::to_string(i);
    Status status = db_->Delete(WriteOptions(), key);
    EXPECT_TRUE(status.ok()) << "Failed to delete key: " << key;
  }

  // 验证删除后的字段数量和内容
  for (size_t i = 0; i < num_fields; ++i) {
    std::string key = "key_" + std::to_string(i);
    Fields fields;
    Status status = db_->GetFields(ReadOptions(), Slice(key), fields);

    if (i < num_fields / 2) {
      EXPECT_FALSE(status.ok()) << "Deleted key still exists: " << key;
    } else {
      EXPECT_TRUE(status.ok()) << "Missing non-deleted key: " << key;
      auto field_value = fields.GetField("field" + std::to_string(i));
      EXPECT_EQ(field_value.second, "value_" + std::to_string(i)) << "Incorrect value for non-deleted field: " << key;
    }
  }
}

// 测试批量更新操作
TEST_F(FieldsTest, TestBulkUpdate) {
  const size_t num_fields = 500;
  leveldb::WriteBatch batch;

  // 准备大量字段数据，并通过 PutFields 插入到数据库
  for (size_t i = 0; i < num_fields; ++i) {
    std::string key = "key_" + std::to_string(i);
    FieldArray fields = {{"field" + std::to_string(i), "old_value_" + std::to_string(i)}};
    Fields f(fields);
    Status status = db_->PutFields(WriteOptions(), Slice(key), f);
    EXPECT_TRUE(status.ok()) << "Failed to put fields for key: " << key;
  }

  // 批量更新一半的字段
  for (size_t i = 0; i < num_fields / 2; ++i) {
    std::string key = "key_" + std::to_string(i);
    FieldArray update_fields = {{"field" + std::to_string(i), "new_value_" + std::to_string(i)}};
    Fields f(update_fields);
    Status status = db_->PutFields(WriteOptions(), Slice(key), f);
    EXPECT_TRUE(status.ok()) << "Failed to update fields for key: " << key;
  }

  // 验证更新后的字段值
  for (size_t i = 0; i < num_fields; ++i) {
    std::string key = "key_" + std::to_string(i);
    Fields fields;
    Status status = db_->GetFields(ReadOptions(), Slice(key), fields);
    EXPECT_TRUE(status.ok()) << "Failed to read key: " << key;

    auto field_value = fields.GetField("field" + std::to_string(i));
    auto expected_value = (i < num_fields / 2) ? ("new_value_" + std::to_string(i)) : ("old_value_" + std::to_string(i));
    EXPECT_EQ(field_value.second, expected_value) << "Incorrect value for updated field: " << key;
  }
}

// 测试批量插入、序列化/反序列化、删除以及 FindKeysByFields 功能
TEST_F(FieldsTest, TestBulkInsertSerializeDeleteAndFindKeys) {
  const size_t num_entries = 500;

  // 准备大量键值对数据，并通过 PutFields 插入到数据库
  for (size_t i = num_entries; i > 0; --i) {
    std::string key = "key_" + std::to_string(i);
    FieldArray fields = {{"field1", "value1_" + std::to_string(i)}, {"field2", "value2_"}};
    Fields ffields(fields);
    Status status = db_->PutFields(WriteOptions(), Slice(key), ffields);
    EXPECT_TRUE(status.ok()) << "Failed to put fields for key: " << key << ", error: " << status.ToString();
  }

  // 验证插入的数据是否正确
  for (size_t i = 1; i <= num_entries; ++i) {
    std::string key = "key_" + std::to_string(i);
    Fields fields;
    Status status = db_->GetFields(ReadOptions(), Slice(key), fields);
    EXPECT_TRUE(status.ok()) << "Failed to read key: " << key << ", error: " << status.ToString();

    // 使用 GetField 方法验证字段值
    auto field1_value = fields.GetField("field1");
    auto field2_value = fields.GetField("field2");

    EXPECT_EQ(field1_value.second, "value1_" + std::to_string(i)) << "Incorrect value for field1 in key: " << key;
    EXPECT_EQ(field2_value.second, "value2_") << "Incorrect value for field2 in key: " << key;
  }

  // 使用 Delete 删除第一个键值对
  Status status = db_->Delete(WriteOptions(), "key_1");
  EXPECT_TRUE(status.ok()) << "Failed to delete key: key_1, error: " << status.ToString();

  // 使用 FindKeysByFields 查找包含特定字段的键
  FieldArray fields_to_find = {{"field2", "value2_"}};

  Options options;
  options.create_if_missing = true;
  DBImpl* impl = new DBImpl(options, dbname_);

  std::vector<std::string> found_keys = Fields::FindKeysByFields(db_, fields_to_find, impl);

  // 验证找到的键是否正确
  EXPECT_EQ(found_keys.size(), num_entries - 1) << "Expected " << num_entries - 1 << " keys but found " << found_keys.size();
  // for (size_t i = 2; i <= num_entries; ++i) {
  //   std::string expected_key = "key_" + std::to_string(i);
  //   EXPECT_TRUE(std::find(found_keys.begin(), found_keys.end(), expected_key) != found_keys.end())
  //       << "Key not found: " << expected_key;
  // }

  // 再次查找，这次没有符合条件的字段
  FieldArray no_match_fields = {{"nonexistent_field", ""}};
  found_keys = Fields::FindKeysByFields(db_, no_match_fields, impl);
  EXPECT_TRUE(found_keys.empty()) << "Expected an empty result for non-matching fields.";
}

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}