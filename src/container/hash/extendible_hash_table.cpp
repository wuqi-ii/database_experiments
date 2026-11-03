//===----------------------------------------------------------------------===//
//
//                         BusTub
//
// extendible_hash_table.cpp
//
// Identification: src/container/hash/extendible_hash_table.cpp
//
// Copyright (c) 2022, Carnegie Mellon University Database Group
//
//===----------------------------------------------------------------------===//

#include <cassert>
#include <cstdlib>
#include <functional>
#include <list>
#include <utility>

#include "container/hash/extendible_hash_table.h"
#include "storage/page/page.h"

namespace bustub {
// dir_是一个动态数组，每个元素保存一个桶的共享指针
template <typename K, typename V>  // bucket_size_(bucket_size)是初始化成员变量，bucket_size_ 是哈希表对象的成员变量
ExtendibleHashTable<K, V>::ExtendibleHashTable(size_t bucket_size) : bucket_size_(bucket_size) {
  dir_.push_back(std::make_shared<Bucket>(bucket_size_, 0));  //创建一个局部深度为0的桶，让第一个目录指向它
}  //会创建一个Bucket对象，make_shared返回此对象

template <typename K, typename V>
auto ExtendibleHashTable<K, V>::IndexOf(const K &key) -> size_t {
  int mask = (1 << global_depth_) - 1;
  return std::hash<K>()(key) & mask;
}

template <typename K, typename V>
auto ExtendibleHashTable<K, V>::GetGlobalDepth() const -> int {
  std::scoped_lock<std::mutex> lock(latch_);
  return GetGlobalDepthInternal();
}

template <typename K, typename V>
auto ExtendibleHashTable<K, V>::GetGlobalDepthInternal() const -> int {
  return global_depth_;
}

template <typename K, typename V>
auto ExtendibleHashTable<K, V>::GetLocalDepth(int dir_index) const -> int {
  std::scoped_lock<std::mutex> lock(latch_);
  return GetLocalDepthInternal(dir_index);
}

template <typename K, typename V>
auto ExtendibleHashTable<K, V>::GetLocalDepthInternal(int dir_index) const -> int {
  return dir_[dir_index]->GetDepth();
}

template <typename K, typename V>
auto ExtendibleHashTable<K, V>::GetNumBuckets() const -> int {
  std::scoped_lock<std::mutex> lock(latch_);
  return GetNumBucketsInternal();
}

template <typename K, typename V>
auto ExtendibleHashTable<K, V>::GetNumBucketsInternal() const -> int {
  return num_buckets_;
}
//整个哈希表的查找
template <typename K, typename V>
auto ExtendibleHashTable<K, V>::Find(const K &key, V &value) -> bool {
  std::scoped_lock<std::mutex> lock(latch_);
  return dir_[IndexOf(key)]->Find(key, value);
}
//哈希表删除
template <typename K, typename V>
auto ExtendibleHashTable<K, V>::Remove(const K &key) -> bool {
  std::scoped_lock<std::mutex> lock(latch_);
  return dir_[IndexOf(key)]->Remove(key);
}

template <typename K, typename V>
void ExtendibleHashTable<K, V>::Insert(const K &key, const V &value) {
  std::scoped_lock<std::mutex> table_lock(latch_);

  while (true) {
    auto target_bucket = dir_[IndexOf(key)];

    if (target_bucket->Insert(key, value)) {
      return;
    }

    int previous_depth = target_bucket->GetDepth();

    // 目录位数不足时，复制目录指针并增加全局深度。
    if (previous_depth == global_depth_) {
      size_t directory_size = dir_.size();

      for (size_t entry_index = 0; entry_index < directory_size; ++entry_index) {
        auto shared_bucket = dir_[entry_index];
        dir_.push_back(shared_bucket);
      }

      ++global_depth_;
    }

    target_bucket->IncrementDepth();
    auto split_bucket = std::make_shared<Bucket>(bucket_size_, target_bucket->GetDepth());
    ++num_buckets_;

    // 用新增的这一位决定目录项是否转向新桶。
    size_t routing_bit = size_t{1} << previous_depth;

    for (size_t entry_index = 0; entry_index < dir_.size(); ++entry_index) {
      if (dir_[entry_index] == target_bucket && (entry_index & routing_bit) != 0) {
        dir_[entry_index] = split_bucket;
      }
    }

    // 移出原有数据，再根据新目录重新分配。
    std::list<std::pair<K, V>> entries_to_redistribute;
    entries_to_redistribute.splice(entries_to_redistribute.end(), target_bucket->GetItems());

    for (const auto &entry : entries_to_redistribute) {
      dir_[IndexOf(entry.first)]->Insert(entry.first, entry.second);
    }
  }
}

//===--------------------------------------------------------------------===//
// Bucket
//===--------------------------------------------------------------------===//
template <typename K, typename V>
ExtendibleHashTable<K, V>::Bucket::Bucket(size_t array_size, int depth) : size_(array_size), depth_(depth) {}

template <typename K, typename V>
auto ExtendibleHashTable<K, V>::Bucket::Find(const K &key, V &value) -> bool {
  //遍历检查键值对，找到输出值
  for (const auto &item : list_) {
    if (item.first == key) {
      value = item.second;
      return true;
    }
  }
  return false;
}

template <typename K, typename V>
auto ExtendibleHashTable<K, V>::Bucket::Remove(const K &key) -> bool {
  for (auto it = list_.begin(); it != list_.end(); ++it) {
    if (it->first == key) {
      list_.erase(it);
      return true;
    }
  }
  return false;
}

template <typename K, typename V>
auto ExtendibleHashTable<K, V>::Bucket::Insert(const K &key, const V &value) -> bool {
  //插入
  for (auto &item : list_) {
    if (item.first == key) {
      item.second = value;
      return true;
    }
  }
  if (IsFull()) {
    return false;
  }
  list_.emplace_back(key, value);  //在链表末尾构造一对键值
  return true;
}

template class ExtendibleHashTable<page_id_t, Page *>;
template class ExtendibleHashTable<Page *, std::list<Page *>::iterator>;
template class ExtendibleHashTable<int, int>;
// test purpose
template class ExtendibleHashTable<int, std::string>;
template class ExtendibleHashTable<int, std::list<int>::iterator>;

}  // namespace bustub
