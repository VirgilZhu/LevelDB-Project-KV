#include <iostream>
#include <gtest/gtest.h>
#include <chrono>
#include <vector>
#include "leveldb/env.h"
#include "leveldb/db.h"
#include "db/fields.h"
#include "leveldb/write_batch.h"

using namespace leveldb;

// Number of key/values to operate in database
constexpr int num_ = 100000;
// Size of each value
constexpr int value_size_ = 1000;
// Number of read operations
constexpr int reads_ = 100000;

Status OpenDB(std::string dbName, DB **db) {
  Options options;
  options.create_if_missing = true;
  return DB::Open(options, dbName, db);
}

void InsertData(DB *db, std::vector<int64_t> &lats) {
  WriteOptions writeOptions;
  srand(0);
  for (int i = 0; i < num_; ++i) {
    int key_ = rand() % num_ + 1;
    std::string key = std::to_string(key_);
    std::string value(value_size_, 'a');
    auto start_time = std::chrono::steady_clock::now();
    db->Put(writeOptions, key, value);
    auto end_time = std::chrono::steady_clock::now();
    lats.emplace_back(std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time).count());
  }
}

void InsertFields(DB *db, std::vector<int64_t> &lats) {
  WriteOptions writeOptions;
  srand(0);
  for (int i = 0; i < num_; ++i) {
    int key_ = rand() % num_ + 1;
    std::string key = std::to_string(key_);
    FieldArray fields = {{"field" + std::to_string(key_), "old_value_" + std::to_string(key)}};
    Fields f(fields);
    auto start_time = std::chrono::steady_clock::now();
    db->PutFields(writeOptions, Slice(key), f);
    auto end_time = std::chrono::steady_clock::now();
    lats.emplace_back(std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time).count());
  }
}

void GetData(DB *db, std::vector<int64_t> &lats) {
  ReadOptions readOptions;
  srand(0);
  for (int i = 0; i < reads_; ++i) {
    int key_ = rand() % num_ + 1;
    std::string key = std::to_string(key_);
    std::string value;
    auto start_time = std::chrono::steady_clock::now();
    db->Get(readOptions, key, &value);
    auto end_time = std::chrono::steady_clock::now();
    lats.emplace_back(std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time).count());
  }
}

void ReadOrdered(DB *db, std::vector<int64_t> &lats) {
  Iterator* iter = db->NewIterator(ReadOptions());
  int i = 0;
  for (iter->SeekToFirst(); i < reads_ && iter->Valid(); iter->Next()) {
    ++i;
    auto start_time = std::chrono::steady_clock::now();
    // Just iterating over the data without performing any operation.
    auto end_time = std::chrono::steady_clock::now();
    lats.emplace_back(std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time).count());
  }
  delete iter;
}

void FindKeys(DB *db, std::vector<int64_t> &lats) {
  srand(0);
  for (int i = 0; i < reads_; ++i) {
    int key_ = rand() % num_ + 1;
    FieldArray fields_to_find = {{"field" + std::to_string(key_), "old_value_" + std::to_string(key_)}};
    auto start_time = std::chrono::steady_clock::now();
    Fields::FindKeysByFields(db, fields_to_find);
    auto end_time = std::chrono::steady_clock::now();
    lats.emplace_back(std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time).count());
  }
}

double CalculatePercentile(const std::vector<int64_t>& latencies, double percentile) {
  if (latencies.empty()) return 0.0;

  std::vector<int64_t> sorted_latencies = latencies;
  std::sort(sorted_latencies.begin(), sorted_latencies.end());

  size_t index = static_cast<size_t>(percentile * sorted_latencies.size());
  if (index >= sorted_latencies.size()) index = sorted_latencies.size() - 1;

  return sorted_latencies[index];
}

template<typename Func>
void RunBenchmark(const char* name, Func func) {
  DB *db;
  if (!OpenDB("testdb_bench", &db).ok()) {
    std::cerr << "open db failed" << std::endl;
    abort();
  }

  std::vector<int64_t> lats;
  auto start_time = std::chrono::steady_clock::now();
  func(db, lats);
  auto end_time = std::chrono::steady_clock::now();
  auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time).count();

  double avg = 0.0;
  for (auto latency : lats) {
    avg += latency;
  }
  avg /= lats.size();

  double p75 = CalculatePercentile(lats, 0.75);
  double p99 = CalculatePercentile(lats, 0.99);

  std::cout << name << " Latency (avg, P75, P99): " << avg << " micros/op, " << p75 << " micros/op, " << p99 << " micros/op" << std::endl;
  std::cout << name << " Throughput: " << lats.size() / duration << " ops/ms" << std::endl;

  delete db;
}

class BenchTest : public ::testing::TestWithParam<double> {};

TEST_P(BenchTest, PutLatency) { RunBenchmark("Put", InsertData); }
TEST_P(BenchTest, PutLatency) { RunBenchmark("PutFields", InsertFields); }
TEST_P(BenchTest, GetLatency) { RunBenchmark("Get", GetData); }
TEST_P(BenchTest, IteratorLatency) { RunBenchmark("Iterator", ReadOrdered); }
TEST_P(BenchTest, FindKeysByFieldLatency) { RunBenchmark("FindKeysByFields", FindKeys); } 


int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}