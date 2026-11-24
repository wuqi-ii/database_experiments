//===----------------------------------------------------------------------===//
//
//                         CMU-DB Project (15-445/645)
//                         ***DO NO SHARE PUBLICLY***
//
// Identification: src/include/index/b_plus_tree.h
//
// Copyright (c) 2018, Carnegie Mellon University Database Group
//
//===----------------------------------------------------------------------===//
#pragma once

#include <queue>
#include <string>
#include <vector>

#include "concurrency/transaction.h"
#include "storage/index/index_iterator.h"
#include "storage/page/b_plus_tree_internal_page.h"
#include "storage/page/b_plus_tree_leaf_page.h"

namespace bustub {

#define BPLUSTREE_TYPE BPlusTree<KeyType, ValueType, KeyComparator>

/**
 * Main class providing the API for the Interactive B+ Tree.
 *
 * Implementation of simple b+ tree data structure where internal pages direct
 * the search and leaf pages contain actual data.
 * (1) We only support unique key
 * (2) support insert & remove
 * (3) The structure should shrink and grow dynamically
 * (4) Implement index iterator for range scan
 */
INDEX_TEMPLATE_ARGUMENTS
class BPlusTree {
  using InternalPage = BPlusTreeInternalPage<KeyType, page_id_t, KeyComparator>;
  using LeafPage = BPlusTreeLeafPage<KeyType, ValueType, KeyComparator>;

 public:
  explicit BPlusTree(std::string name, BufferPoolManager *buffer_pool_manager, const KeyComparator &comparator,
                     int leaf_max_size = LEAF_PAGE_SIZE, int internal_max_size = INTERNAL_PAGE_SIZE);

  // Returns true if this B+ tree has no keys and values.
  auto IsEmpty() const -> bool;

  // Insert a key-value pair into this B+ tree.
  auto Insert(const KeyType &key, const ValueType &value, Transaction *transaction = nullptr) -> bool;

  // Remove a key and its value from this B+ tree.
  void Remove(const KeyType &key, Transaction *transaction = nullptr);

  // return the value associated with a given key
  auto GetValue(const KeyType &key, std::vector<ValueType> *result, Transaction *transaction = nullptr) -> bool;

  // return the page id of the root node
  auto GetRootPageId() -> page_id_t;

  // index iterator
  auto Begin() -> INDEXITERATOR_TYPE;
  auto Begin(const KeyType &key) -> INDEXITERATOR_TYPE;
  auto End() -> INDEXITERATOR_TYPE;

  // print the B+ tree
  void Print(BufferPoolManager *bpm);

  // draw the B+ tree
  void Draw(BufferPoolManager *bpm, const std::string &outf);

  // read data from file and insert one by one
  void InsertFromFile(const std::string &file_name, Transaction *transaction = nullptr);

  // read data from file and remove one by one
  void RemoveFromFile(const std::string &file_name, Transaction *transaction = nullptr);

 private:
  // 分工文档约定的操作类型，FindLeafPage 据此决定锁策略（单线程阶段忽略）
  enum class Operation { SEARCH, INSERT, REMOVE };

  void UpdateRootPageId(int insert_record = 0);

  /* ---------- A 负责实现 ---------- */
  // 返回已固定的叶子 Page *，调用者负责 Unpin；leftmost 用于寻找最左叶子
  auto FindLeafPage(const KeyType &key, Operation operation, Transaction *transaction, bool leftmost = false)
      -> Page *;
  void StartNewTree(const KeyType &key, const ValueType &value);
  auto SplitLeaf(LeafPage *leaf) -> Page *;
  auto SplitInternal(InternalPage *internal, KeyType *separator) -> Page *;
  void InsertIntoParent(BPlusTreePage *left, const KeyType &separator, BPlusTreePage *right,
                        Transaction *transaction);
  // 统一根编号入口：内存值 + 头部页记录
  void SetRootPageId(page_id_t new_root_id);
  // 更新子页的父编号，单线程下自行 Fetch / Unpin
  void SetChildParent(page_id_t child_id, page_id_t parent_id, Transaction *transaction);

  /* ---------- B 负责实现（A 在此统一声明） ---------- */
  void RebalanceAfterDelete(BPlusTreePage *node, Transaction *transaction);
  void AdjustRoot(BPlusTreePage *root, Transaction *transaction);

  /* Debug Routines for FREE!! */
  void ToGraph(BPlusTreePage *page, BufferPoolManager *bpm, std::ofstream &out) const;

  void ToString(BPlusTreePage *page, BufferPoolManager *bpm) const;

  // member variable
  std::string index_name_;
  page_id_t root_page_id_;
  BufferPoolManager *buffer_pool_manager_;
  KeyComparator comparator_;
  int leaf_max_size_;
  int internal_max_size_;
  // 头部页是否已经为本索引创建过 <name, root_id> 记录
  bool root_record_created_{false};
};

}  // namespace bustub
