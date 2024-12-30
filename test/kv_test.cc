#include <chrono>

#include "gtest/gtest.h"
#include "leveldb/env.h"
#include "leveldb/db.h"

using namespace leveldb;

constexpr int short_value_size = 4;
constexpr int long_value_size = 32;
constexpr int data_size = 512;

Status OpenDB(std::string dbName, DB **db) {
  std::string rm_command = "rm -rf " + dbName;
  system(rm_command.c_str());
  
  Options options;
  options.create_if_missing = true;
  return DB::Open(options, dbName, db);
}

void InsertData(DB *db, int value_size) {
  WriteOptions writeOptions;
  int key_num = data_size / value_size;
  srand(42);

  for (int i = 0; i < key_num; i++) {
    int key_ = rand() % key_num+1;
    std::string key = std::to_string(key_);
    std::string value(value_size, 'a');
  }

}

void GetData(DB *db, int size = (1 << 30), int value_size = 0) {
  ReadOptions readOptions;
  int key_num = data_size / value_size;
  
  // 点查
  srand(42);
  for (int i = 0; i < 100; i++) {
    int key_ = rand() % key_num+1;
    std::string key = std::to_string(key_);
    std::string value;
    db->Get(readOptions, key, &value);
  }
}

TEST(TestTTL, GetValue) {
    DB *db;
    if(OpenDB("testdb_ReadTTL", &db).ok() == false) {
        std::cerr << "open db failed" << std::endl;
        abort();
    }
    InsertData(db, short_value_size);

    ReadOptions readOptions;
    Status status;
    int key_num = data_size / short_value_size;
    srand(42);
    for (int i = 0; i < 100; i++) {
        int key_ = rand() % key_num+1;
        std::string key = std::to_string(key_);
        std::string value;
        std::string expected_value(short_value_size, 'a');
        status = db->Get(readOptions, key, &value);
        ASSERT_TRUE(status.ok());
        EXPECT_EQ(expected_value, value);
    }
}

TEST(TestTTL, GetLongValue) {
    DB *db;
    if(OpenDB("testdb_ReadTTL", &db).ok() == false) {
        std::cerr << "open db failed" << std::endl;
        abort();
    }
    InsertData(db, long_value_size);

    ReadOptions readOptions;
    Status status;
    int key_num = data_size / long_value_size;
    srand(42);
    for (int i = 0; i < 100; i++) {
        int key_ = rand() % key_num+1;
        std::string key = std::to_string(key_);
        std::string value;
        std::string expected_value(long_value_size, 'a');
        status = db->Get(readOptions, key, &value);
        ASSERT_TRUE(status.ok());
        EXPECT_EQ(expected_value, value);
    }
}


int main(int argc, char** argv) {
  // All tests currently run with the same read-only file limits.
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}