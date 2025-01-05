#include "kv_separate_management.h"
#include <queue>
#include <vector>
#include <db/dbformat.h>

namespace leveldb {

bool SeparateManagement::ConvertQueue(uint64_t& db_sequence) {
    if (!need_updates_.empty()) {
        db_sequence++;
    } else {
        return false;
    }
    while (!need_updates_.empty()) {
        ValueLogInfo* info = need_updates_.front();
        need_updates_.pop_front();
        map_file_info_.erase(info->logfile_number_);
        info->last_sequence_ = db_sequence;
        db_sequence += info->left_kv_numbers_;
        garbage_collection_.push_back(info);
    }
    assert(db_sequence <= kMaxSequenceNumber);
    return true;
}

void SeparateManagement::WriteFileMap(uint64_t fid, int kv_numbers, size_t log_memory) {
    assert(map_file_info_.find(fid) == map_file_info_.end());
    ValueLogInfo* info = new ValueLogInfo();
    info->logfile_number_ = fid;
    info->left_kv_numbers_ = kv_numbers;
    assert(kv_numbers <= kMaxSequenceNumber);
    info->invalid_memory_ = 0;
    info->last_sequence_ = -1;
    info->file_size_ = log_memory;
    map_file_info_.insert(std::make_pair(fid,info));
}

void SeparateManagement::UpdateMap(uint64_t fid, uint64_t abandon_memory) {
    if (map_file_info_.find(fid) != map_file_info_.end()) {
        ValueLogInfo* info = map_file_info_[fid];
        info->left_kv_numbers_--;
        info->invalid_memory_ += abandon_memory;
    }
}

void SeparateManagement::UpdateQueue(uint64_t fid) {
    std::priority_queue<ValueLogInfo*, std::vector<ValueLogInfo*>, MapCmp> sort_priority_;

    for (auto iter = map_file_info_.begin(); iter != map_file_info_.end(); ++iter) {
        if (delete_files_.find( iter->first) == delete_files_.end()) {
            sort_priority_.push(iter->second);
        }
    }
    /* 默认每次只把一个 VLog 加入到 GC 队列 */
    int num = 1;
    int threshold = garbage_collection_threshold_;
    if (!sort_priority_.empty()
        && sort_priority_.top()->invalid_memory_ >= garbage_collection_threshold_ * 1.2) {
        /* 如果无效空间最多的 VLog 超过 GC 阈值 20%，这次会把 1~3 个 VLog 加入到 GC 队列  */
        num = 3;
        threshold = garbage_collection_threshold_ * 1.2;
    }
    while (!sort_priority_.empty() && num > 0) {
        ValueLogInfo* info = sort_priority_.top();
        sort_priority_.pop();
        /* 优先删除较旧的 VLog */
        if (info->logfile_number_ > fid) {
            continue;
        }
        num--;
        if (info->invalid_memory_ >= threshold) {
            need_updates_.push_back(info);
            /* 更新准备 GC（准备删除）的 VLog */
            delete_files_.insert(info->logfile_number_);
        }
    }
}

bool SeparateManagement::GetGarbageCollectionQueue(uint64_t& fid, uint64_t& last_sequence) {
    if (garbage_collection_.empty()) {
        return false;
    } else {
        ValueLogInfo* info = garbage_collection_.front();
        garbage_collection_.pop_front();
        fid = info->logfile_number_;
        last_sequence = info->last_sequence_;
        return true;
    }
}

void SeparateManagement::CollectionMap(){
    if (map_file_info_.empty()) return;

    for (auto iter : map_file_info_) {
        uint64_t fid  = iter.first;
        ValueLogInfo* info = iter.second;
        if (delete_files_.find(fid) == delete_files_.end()) {
            need_updates_.push_back(info);
            delete_files_.insert(info->logfile_number_);
        }
    }
}

}