#include "gtest/gtest.h"
#include "leveldb/db.h"
#include "db/fields.h"


using namespace leveldb;


constexpr int value_size = 2048;
constexpr int data_size = 128 << 20;


Status OpenDB(std::string dbName, DB **db) {
  Options options;
  options.create_if_missing = true;
  return DB::Open(options, dbName, db);
}


TEST(TestValueField, ReadValueField) {
  DB *db;

  if(OpenDB("testdb", &db).ok() == false) {
    std::cerr << "open db failed" << std::endl;
    abort();
  }

  std::string key = "k_1";

  FieldArray fields = {
      {"name", "Customer#000000001"},
      {"address", "IVhzIApeRb"},
      {"phone", "25-989-741-2988"}
  };

  // 序列化并插入
  std::string value = SerializeValue(fields);
  db->Put(WriteOptions(), key, value);

  // 读取并反序列化
  std::string value_ret;
  db->Get(ReadOptions(), key, &value_ret);
  auto fields_ret = ParseValue(value_ret);




}


int main(int argc, char** argv) {
  // All tests currently run with the same read-only file limits.
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
