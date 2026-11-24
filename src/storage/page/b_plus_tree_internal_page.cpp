//===----------------------------------------------------------------------===//
//
//                         CMU-DB Project (15-445/645)
//                         ***DO NO SHARE PUBLICLY***
//
// Identification: src/page/b_plus_tree_internal_page.cpp
//
// Copyright (c) 2018, Carnegie Mellon University Database Group
//
//===----------------------------------------------------------------------===//

#include <iostream>
#include <sstream>

#include "common/exception.h"
#include "storage/page/b_plus_tree_internal_page.h"

namespace bustub {
/*****************************************************************************
 * HELPER METHODS AND UTILITIES
 *****************************************************************************/
/*
 * Init method after creating a new internal page
 * Including set page type, set current size, set page id, set parent id and set
 * max page size
 */
INDEX_TEMPLATE_ARGUMENTS
void B_PLUS_TREE_INTERNAL_PAGE_TYPE::Init(page_id_t page_id, page_id_t parent_id, int max_size) {
  SetPageType(IndexPageType::INTERNAL_PAGE);
  SetSize(0);
  SetMaxSize(max_size);
  SetPageId(page_id);
  SetParentPageId(parent_id);
  SetLSN();
}

/*
 * Helper method to get/set the key associated with input "index"(a.k.a
 * array offset)
 */
INDEX_TEMPLATE_ARGUMENTS
auto B_PLUS_TREE_INTERNAL_PAGE_TYPE::KeyAt(int index) const -> KeyType { return array_[index].first; }

INDEX_TEMPLATE_ARGUMENTS
void B_PLUS_TREE_INTERNAL_PAGE_TYPE::SetKeyAt(int index, const KeyType &key) { array_[index].first = key; }

/*
 * Helper method to get the value associated with input "index"(a.k.a array
 * offset)
 */
INDEX_TEMPLATE_ARGUMENTS
auto B_PLUS_TREE_INTERNAL_PAGE_TYPE::ValueAt(int index) const -> ValueType { return array_[index].second; }

INDEX_TEMPLATE_ARGUMENTS
auto B_PLUS_TREE_INTERNAL_PAGE_TYPE::Lookup(const KeyType &key, const KeyComparator &comparator) const -> ValueType {
  // 第 0 个键无效，从 1 开始二分；相等键进入该分隔键对应的右侧子页。
  int left = 1;
  int right = GetSize() - 1;
  while (left <= right) {
    int mid = left + (right - left) / 2;
    if (comparator(array_[mid].first, key) <= 0) {
      left = mid + 1;
    } else {
      right = mid - 1;
    }
  }
  // right 是最后一个 key >= array_[right].first 的下标，right==0 表示走最左子页。
  return array_[right].second;
}

INDEX_TEMPLATE_ARGUMENTS
auto B_PLUS_TREE_INTERNAL_PAGE_TYPE::ValueIndex(const ValueType &value) const -> int {
  for (int i = 0; i < GetSize(); i++) {
    if (array_[i].second == value) {
      return i;
    }
  }
  return -1;
}

INDEX_TEMPLATE_ARGUMENTS
void B_PLUS_TREE_INTERNAL_PAGE_TYPE::PopulateNewRoot(const ValueType &left_child, const KeyType &key,
                                                     const ValueType &right_child) {
  array_[0].first = KeyType{};  // 第 0 个键槽无效
  array_[0].second = left_child;
  array_[1].first = key;
  array_[1].second = right_child;
  SetSize(2);
}

INDEX_TEMPLATE_ARGUMENTS
void B_PLUS_TREE_INTERNAL_PAGE_TYPE::InsertNodeAfter(const ValueType &old_child, const KeyType &new_key,
                                                     const ValueType &new_child) {
  int pos = ValueIndex(old_child);
  // 调用前已保证有空位，从尾部向后搬运。
  for (int i = GetSize() - 1; i > pos; i--) {
    array_[i + 1] = array_[i];
  }
  array_[pos + 1].first = new_key;
  array_[pos + 1].second = new_child;
  IncreaseSize(1);
}

INDEX_TEMPLATE_ARGUMENTS
void B_PLUS_TREE_INTERNAL_PAGE_TYPE::Remove(int index) {
  for (int i = index; i < GetSize() - 1; i++) {
    array_[i] = array_[i + 1];
  }
  IncreaseSize(-1);
  if (GetSize() > 0) {
    array_[0].first = KeyType{};  // 保证新的第 0 键槽无效
  }
}

INDEX_TEMPLATE_ARGUMENTS
auto B_PLUS_TREE_INTERNAL_PAGE_TYPE::MoveHalfTo(BPlusTreeInternalPage *recipient) -> KeyType {
  int total = GetSize();
  int keep = total / 2;
  int move = total - keep;
  KeyType separator = array_[keep].first;  // 右半部分第一个键即提升键
  for (int i = 0; i < move; i++) {
    recipient->array_[i] = array_[keep + i];
  }
  recipient->SetSize(move);
  recipient->array_[0].first = KeyType{};  // 接收页的第 0 键槽无效
  SetSize(keep);
  return separator;
}

INDEX_TEMPLATE_ARGUMENTS
void B_PLUS_TREE_INTERNAL_PAGE_TYPE::MoveAllTo(BPlusTreeInternalPage *recipient, const KeyType &parent_key) {
  // 当前页是右页，接收页是左页；parent_key 成为右页首个子指针的分隔键。
  int left_size = recipient->GetSize();
  recipient->array_[left_size].first = parent_key;
  recipient->array_[left_size].second = array_[0].second;
  for (int i = 1; i < GetSize(); i++) {
    recipient->array_[left_size + i] = array_[i];
  }
  recipient->SetSize(left_size + GetSize());
  SetSize(0);
}

INDEX_TEMPLATE_ARGUMENTS
auto B_PLUS_TREE_INTERNAL_PAGE_TYPE::MoveFirstToEndOf(BPlusTreeInternalPage *recipient,
                                                      const KeyType &parent_key) -> KeyType {
  // 当前是右页，借出最左子指针给左页末尾；旧父分隔键连接被借出的子指针。
  KeyType new_parent_key = array_[1].first;
  ValueType moved_child = array_[0].second;
  recipient->array_[recipient->GetSize()].first = parent_key;
  recipient->array_[recipient->GetSize()].second = moved_child;
  recipient->IncreaseSize(1);
  for (int i = 0; i < GetSize() - 1; i++) {
    array_[i] = array_[i + 1];
  }
  IncreaseSize(-1);
  array_[0].first = KeyType{};  // 新的第 0 键槽无效
  return new_parent_key;
}

INDEX_TEMPLATE_ARGUMENTS
auto B_PLUS_TREE_INTERNAL_PAGE_TYPE::MoveLastToFrontOf(BPlusTreeInternalPage *recipient,
                                                       const KeyType &parent_key) -> KeyType {
  // 当前是左页，借出最右子指针到右页开头；被借出子指针的原键成为新父分隔键。
  KeyType new_parent_key = array_[GetSize() - 1].first;
  ValueType moved_child = array_[GetSize() - 1].second;
  IncreaseSize(-1);
  for (int i = recipient->GetSize(); i > 0; i--) {
    recipient->array_[i] = recipient->array_[i - 1];
  }
  recipient->array_[0].first = KeyType{};
  recipient->array_[0].second = moved_child;
  recipient->array_[1].first = parent_key;  // 旧父分隔键下移，连接原来的最左子指针
  recipient->IncreaseSize(1);
  return new_parent_key;
}

// valuetype for internalNode should be page id_t
template class BPlusTreeInternalPage<GenericKey<4>, page_id_t, GenericComparator<4>>;
template class BPlusTreeInternalPage<GenericKey<8>, page_id_t, GenericComparator<8>>;
template class BPlusTreeInternalPage<GenericKey<16>, page_id_t, GenericComparator<16>>;
template class BPlusTreeInternalPage<GenericKey<32>, page_id_t, GenericComparator<32>>;
template class BPlusTreeInternalPage<GenericKey<64>, page_id_t, GenericComparator<64>>;
}  // namespace bustub
