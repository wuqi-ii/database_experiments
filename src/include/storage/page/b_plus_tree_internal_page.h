//===----------------------------------------------------------------------===//
//
//                         CMU-DB Project (15-445/645)
//                         ***DO NO SHARE PUBLICLY***
//
// Identification: src/include/page/b_plus_tree_internal_page.h
//
// Copyright (c) 2018, Carnegie Mellon University Database Group
//
//===----------------------------------------------------------------------===//
#pragma once

#include <queue>

#include "storage/page/b_plus_tree_page.h"

namespace bustub {

#define B_PLUS_TREE_INTERNAL_PAGE_TYPE BPlusTreeInternalPage<KeyType, ValueType, KeyComparator>
#define INTERNAL_PAGE_HEADER_SIZE 24
#define INTERNAL_PAGE_SIZE ((BUSTUB_PAGE_SIZE - INTERNAL_PAGE_HEADER_SIZE) / (sizeof(MappingType)))
/**
 * Store n indexed keys and n+1 child pointers (page_id) within internal page.
 * Pointer PAGE_ID(i) points to a subtree in which all keys K satisfy:
 * K(i) <= K < K(i+1).
 * NOTE: since the number of keys does not equal to number of child pointers,
 * the first key always remains invalid. That is to say, any search/lookup
 * should ignore the first key.
 *
 * Internal page format (keys are stored in increasing order):
 *  --------------------------------------------------------------------------
 * | HEADER | KEY(1)+PAGE_ID(1) | KEY(2)+PAGE_ID(2) | ... | KEY(n)+PAGE_ID(n) |
 *  --------------------------------------------------------------------------
 */
INDEX_TEMPLATE_ARGUMENTS
class BPlusTreeInternalPage : public BPlusTreePage {
 public:
  // must call initialize method after "create" a new node
  void Init(page_id_t page_id, page_id_t parent_id = INVALID_PAGE_ID, int max_size = INTERNAL_PAGE_SIZE);

  auto KeyAt(int index) const -> KeyType;
  void SetKeyAt(int index, const KeyType &key);
  auto ValueAt(int index) const -> ValueType;

  /* 本次新增的内部页辅助接口（A 实现，供 B 的删除流程调用） */
  // 忽略第 0 个无效 key，二分返回目标 key 应进入的子页面编号
  auto Lookup(const KeyType &key, const KeyComparator &comparator) const -> ValueType;
  // 返回指定子页面编号所在下标，未找到返回 -1
  auto ValueIndex(const ValueType &value) const -> int;
  // 新建根：两个子指针 + 一个有效分隔键，size 置为 2
  void PopulateNewRoot(const ValueType &left_child, const KeyType &key, const ValueType &right_child);
  // 在 old_child 后插入新键与子指针，调用前保证有空位
  void InsertNodeAfter(const ValueType &old_child, const KeyType &new_key, const ValueType &new_child);
  // 删除指定下标的子指针条目并左移，保证新的第 0 键槽无效
  void Remove(int index);

  // 分裂：原页保留前 size/2 个子指针，其余移到空接收页；返回提升键
  auto MoveHalfTo(BPlusTreeInternalPage *recipient) -> KeyType;
  // 合并：当前页是右页，接收页是左页，使用父分隔键 parent_key 连接
  void MoveAllTo(BPlusTreeInternalPage *recipient, const KeyType &parent_key);
  // 当前右页借出最左子指针给左页，返回应写回父页的新分隔键
  auto MoveFirstToEndOf(BPlusTreeInternalPage *recipient, const KeyType &parent_key) -> KeyType;
  // 当前左页借出最右子指针给右页，返回应写回父页的新分隔键
  auto MoveLastToFrontOf(BPlusTreeInternalPage *recipient, const KeyType &parent_key) -> KeyType;

 private:
  // Flexible array member for page data.
  MappingType array_[1];
};
}  // namespace bustub
