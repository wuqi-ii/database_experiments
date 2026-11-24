#include <string>

#include "common/exception.h"
#include "common/logger.h"
#include "common/rid.h"
#include "storage/index/b_plus_tree.h"
#include "storage/page/header_page.h"

namespace bustub {
INDEX_TEMPLATE_ARGUMENTS
BPLUSTREE_TYPE::BPlusTree(std::string name, BufferPoolManager *buffer_pool_manager, const KeyComparator &comparator,
                          int leaf_max_size, int internal_max_size)
    : index_name_(std::move(name)),
      root_page_id_(INVALID_PAGE_ID),
      buffer_pool_manager_(buffer_pool_manager),
      comparator_(comparator),
      leaf_max_size_(leaf_max_size),
      internal_max_size_(internal_max_size) {}

/*
 * Helper function to decide whether current b+tree is empty
 */
INDEX_TEMPLATE_ARGUMENTS
auto BPLUSTREE_TYPE::IsEmpty() const -> bool { return root_page_id_ == INVALID_PAGE_ID; }
/*****************************************************************************
 * SEARCH
 *****************************************************************************/
/*
 * Return the only value that associated with input key
 * This method is used for point query
 * @return : true means key exists
 */
INDEX_TEMPLATE_ARGUMENTS
auto BPLUSTREE_TYPE::GetValue(const KeyType &key, std::vector<ValueType> *result, Transaction *transaction) -> bool {
  (void)transaction;
  if (IsEmpty()) {
    return false;
  }
  Page *leaf_page = FindLeafPage(key, Operation::SEARCH, transaction);
  if (leaf_page == nullptr) {
    return false;
  }
  auto *leaf = reinterpret_cast<LeafPage *>(leaf_page->GetData());
  ValueType value{};
  bool found = leaf->Lookup(key, &value, comparator_);
  if (found) {
    result->push_back(value);
  }
  buffer_pool_manager_->UnpinPage(leaf_page->GetPageId(), false);
  return found;
}

/*****************************************************************************
 * INSERTION
 *****************************************************************************/
/*
 * Insert constant key & value pair into b+ tree
 * if current tree is empty, start new tree, update root page id and insert
 * entry, otherwise insert into leaf page.
 * @return: since we only support unique key, if user try to insert duplicate
 * keys return false, otherwise return true.
 */
INDEX_TEMPLATE_ARGUMENTS
auto BPLUSTREE_TYPE::Insert(const KeyType &key, const ValueType &value, Transaction *transaction) -> bool {
  if (IsEmpty()) {
    StartNewTree(key, value);
    return true;
  }
  Page *leaf_page = FindLeafPage(key, Operation::INSERT, transaction);
  if (leaf_page == nullptr) {
    throw Exception(ExceptionType::OUT_OF_MEMORY, "failed to fetch leaf page for insert");
  }
  auto *leaf = reinterpret_cast<LeafPage *>(leaf_page->GetData());
  ValueType existing{};
  if (leaf->Lookup(key, &existing, comparator_)) {
    buffer_pool_manager_->UnpinPage(leaf_page->GetPageId(), false);
    return false;  // 唯一键：重复键返回 false，保留原有值
  }
  if (!leaf->Insert(key, value, comparator_)) {
    buffer_pool_manager_->UnpinPage(leaf_page->GetPageId(), false);
    return false;
  }
  // 叶子稳定状态 size < max_size：插入使 size 达到 max_size 即分裂
  if (leaf->GetSize() == leaf->GetMaxSize()) {
    Page *new_page = SplitLeaf(leaf);
    auto *new_leaf = reinterpret_cast<LeafPage *>(new_page->GetData());
    KeyType separator = new_leaf->KeyAt(0);
    InsertIntoParent(leaf, separator, new_leaf, transaction);
    buffer_pool_manager_->UnpinPage(new_page->GetPageId(), true);
  }
  buffer_pool_manager_->UnpinPage(leaf_page->GetPageId(), true);
  return true;
}

/*****************************************************************************
 * REMOVE
 *****************************************************************************/
/*
 * Delete key & value pair associated with input key
 * If current tree is empty, return immediately.
 * If not, User needs to first find the right leaf page as deletion target, then
 * delete entry from leaf page. Remember to deal with redistribute or merge if
 * necessary.
 */
INDEX_TEMPLATE_ARGUMENTS
void BPLUSTREE_TYPE::Remove(const KeyType &key, Transaction *transaction) {}

/*****************************************************************************
 * INDEX ITERATOR
 *****************************************************************************/
/*
 * Input parameter is void, find the leftmost leaf page first, then construct
 * index iterator
 * @return : index iterator
 */
INDEX_TEMPLATE_ARGUMENTS
auto BPLUSTREE_TYPE::Begin() -> INDEXITERATOR_TYPE { return INDEXITERATOR_TYPE(); }

/*
 * Input parameter is low key, find the leaf page that contains the input key
 * first, then construct index iterator
 * @return : index iterator
 */
INDEX_TEMPLATE_ARGUMENTS
auto BPLUSTREE_TYPE::Begin(const KeyType &key) -> INDEXITERATOR_TYPE { return INDEXITERATOR_TYPE(); }

/*
 * Input parameter is void, construct an index iterator representing the end
 * of the key/value pair in the leaf node
 * @return : index iterator
 */
INDEX_TEMPLATE_ARGUMENTS
auto BPLUSTREE_TYPE::End() -> INDEXITERATOR_TYPE { return INDEXITERATOR_TYPE(); }

/**
 * @return Page id of the root of this tree
 */
INDEX_TEMPLATE_ARGUMENTS
auto BPLUSTREE_TYPE::GetRootPageId() -> page_id_t { return root_page_id_; }

/*****************************************************************************
 * UTILITIES AND DEBUG
 *****************************************************************************/
/*
 * Update/Insert root page id in header page(where page_id = 0, header_page is
 * defined under include/page/header_page.h)
 * Call this method everytime root page id is changed.
 * @parameter: insert_record      default value is false. When set to true,
 * insert a record <index_name, root_page_id> into header page instead of
 * updating it.
 */
INDEX_TEMPLATE_ARGUMENTS
void BPLUSTREE_TYPE::UpdateRootPageId(int insert_record) {
  auto *header_page = static_cast<HeaderPage *>(buffer_pool_manager_->FetchPage(HEADER_PAGE_ID));
  if (insert_record != 0) {
    // create a new record<index_name + root_page_id> in header_page
    header_page->InsertRecord(index_name_, root_page_id_);
  } else {
    // update root_page_id in header_page
    header_page->UpdateRecord(index_name_, root_page_id_);
  }
  buffer_pool_manager_->UnpinPage(HEADER_PAGE_ID, true);
}

/*****************************************************************************
 * A: SHARED HELPERS (single-threaded implementation)
 *****************************************************************************/
INDEX_TEMPLATE_ARGUMENTS
auto BPLUSTREE_TYPE::FindLeafPage(const KeyType &key, Operation operation, Transaction *transaction, bool leftmost)
    -> Page * {
  // 单线程阶段忽略 operation 与 transaction，C 接入并发时在此扩展 latch crabbing
  (void)operation;
  (void)transaction;
  if (IsEmpty()) {
    return nullptr;
  }
  Page *page = buffer_pool_manager_->FetchPage(root_page_id_);
  if (page == nullptr) {
    throw Exception(ExceptionType::OUT_OF_MEMORY, "failed to fetch root page");
  }
  auto *node = reinterpret_cast<BPlusTreePage *>(page->GetData());
  while (!node->IsLeafPage()) {
    auto *internal = reinterpret_cast<InternalPage *>(node);
    page_id_t child_id = leftmost ? internal->ValueAt(0) : internal->Lookup(key, comparator_);
    Page *child_page = buffer_pool_manager_->FetchPage(child_id);
    if (child_page == nullptr) {
      buffer_pool_manager_->UnpinPage(page->GetPageId(), false);
      throw Exception(ExceptionType::OUT_OF_MEMORY, "failed to fetch child page");
    }
    buffer_pool_manager_->UnpinPage(page->GetPageId(), false);
    page = child_page;
    node = reinterpret_cast<BPlusTreePage *>(page->GetData());
  }
  return page;  // 返回时仍持有一个 pin，由调用者释放
}

INDEX_TEMPLATE_ARGUMENTS
void BPLUSTREE_TYPE::StartNewTree(const KeyType &key, const ValueType &value) {
  page_id_t new_page_id;
  Page *page = buffer_pool_manager_->NewPage(&new_page_id);
  if (page == nullptr) {
    throw Exception(ExceptionType::OUT_OF_MEMORY, "failed to allocate root leaf page");
  }
  auto *leaf = reinterpret_cast<LeafPage *>(page->GetData());
  leaf->Init(new_page_id, INVALID_PAGE_ID, leaf_max_size_);
  leaf->Insert(key, value, comparator_);
  SetRootPageId(new_page_id);
  buffer_pool_manager_->UnpinPage(new_page_id, true);
}

INDEX_TEMPLATE_ARGUMENTS
auto BPLUSTREE_TYPE::SplitLeaf(LeafPage *leaf) -> Page * {
  page_id_t new_page_id;
  Page *new_page = buffer_pool_manager_->NewPage(&new_page_id);
  if (new_page == nullptr) {
    throw Exception(ExceptionType::OUT_OF_MEMORY, "failed to allocate new leaf page");
  }
  auto *new_leaf = reinterpret_cast<LeafPage *>(new_page->GetData());
  new_leaf->Init(new_page_id, leaf->GetParentPageId(), leaf->GetMaxSize());
  leaf->MoveHalfTo(new_leaf);  // B 提供：搬移数据并维护叶子链表
  return new_page;             // 返回时仍持有一个 pin
}

INDEX_TEMPLATE_ARGUMENTS
auto BPLUSTREE_TYPE::SplitInternal(InternalPage *internal, KeyType *separator) -> Page * {
  // 在满内部页新增条目前调用
  page_id_t new_page_id;
  Page *new_page = buffer_pool_manager_->NewPage(&new_page_id);
  if (new_page == nullptr) {
    throw Exception(ExceptionType::OUT_OF_MEMORY, "failed to allocate new internal page");
  }
  auto *new_internal = reinterpret_cast<InternalPage *>(new_page->GetData());
  new_internal->Init(new_page_id, internal->GetParentPageId(), internal->GetMaxSize());
  *separator = internal->MoveHalfTo(new_internal);
  // 移到新内部页的子页需要更新父编号
  for (int i = 0; i < new_internal->GetSize(); i++) {
    SetChildParent(new_internal->ValueAt(i), new_page_id, nullptr);
  }
  return new_page;
}

INDEX_TEMPLATE_ARGUMENTS
void BPLUSTREE_TYPE::InsertIntoParent(BPlusTreePage *left, const KeyType &separator, BPlusTreePage *right,
                                      Transaction *transaction) {
  if (left->IsRootPage()) {
    page_id_t new_root_id;
    Page *new_root_page = buffer_pool_manager_->NewPage(&new_root_id);
    if (new_root_page == nullptr) {
      throw Exception(ExceptionType::OUT_OF_MEMORY, "failed to allocate new root page");
    }
    auto *new_root = reinterpret_cast<InternalPage *>(new_root_page->GetData());
    new_root->Init(new_root_id, INVALID_PAGE_ID, internal_max_size_);
    new_root->PopulateNewRoot(left->GetPageId(), separator, right->GetPageId());
    SetChildParent(left->GetPageId(), new_root_id, transaction);
    SetChildParent(right->GetPageId(), new_root_id, transaction);
    SetRootPageId(new_root_id);
    buffer_pool_manager_->UnpinPage(new_root_id, true);
    return;
  }

  page_id_t parent_id = left->GetParentPageId();
  Page *parent_page = buffer_pool_manager_->FetchPage(parent_id);
  if (parent_page == nullptr) {
    throw Exception(ExceptionType::OUT_OF_MEMORY, "failed to fetch parent page");
  }
  auto *parent = reinterpret_cast<InternalPage *>(parent_page->GetData());
  if (parent->GetSize() < parent->GetMaxSize()) {
    parent->InsertNodeAfter(left->GetPageId(), separator, right->GetPageId());
    right->SetParentPageId(parent_id);
    buffer_pool_manager_->UnpinPage(parent_id, true);
    return;
  }

  // 父页已满：先分裂再插入，避免向占满的数组追加
  KeyType parent_separator;
  Page *new_parent_page = SplitInternal(parent, &parent_separator);
  auto *new_parent = reinterpret_cast<InternalPage *>(new_parent_page->GetData());
  // left 一定在分裂前的父页中，分裂后按它落在哪一侧插入新子指针
  if (parent->ValueIndex(left->GetPageId()) != -1) {
    parent->InsertNodeAfter(left->GetPageId(), separator, right->GetPageId());
    right->SetParentPageId(parent->GetPageId());
  } else {
    new_parent->InsertNodeAfter(left->GetPageId(), separator, right->GetPageId());
    right->SetParentPageId(new_parent->GetPageId());
  }
  InsertIntoParent(parent, parent_separator, new_parent, transaction);
  buffer_pool_manager_->UnpinPage(parent->GetPageId(), true);
  buffer_pool_manager_->UnpinPage(new_parent->GetPageId(), true);
}

INDEX_TEMPLATE_ARGUMENTS
void BPLUSTREE_TYPE::SetRootPageId(page_id_t new_root_id) {
  root_page_id_ = new_root_id;
  UpdateRootPageId(root_record_created_ ? 0 : 1);
  root_record_created_ = true;
}

INDEX_TEMPLATE_ARGUMENTS
void BPLUSTREE_TYPE::SetChildParent(page_id_t child_id, page_id_t parent_id, Transaction *transaction) {
  (void)transaction;  // C 接入并发时复用已持页面
  Page *child_page = buffer_pool_manager_->FetchPage(child_id);
  if (child_page == nullptr) {
    throw Exception(ExceptionType::OUT_OF_MEMORY, "failed to fetch child page to update parent");
  }
  auto *child = reinterpret_cast<BPlusTreePage *>(child_page->GetData());
  child->SetParentPageId(parent_id);
  buffer_pool_manager_->UnpinPage(child_id, true);
}

/*
 * This method is used for test only
 * Read data from file and insert one by one
 */
INDEX_TEMPLATE_ARGUMENTS
void BPLUSTREE_TYPE::InsertFromFile(const std::string &file_name, Transaction *transaction) {
  int64_t key;
  std::ifstream input(file_name);
  while (input) {
    input >> key;

    KeyType index_key;
    index_key.SetFromInteger(key);
    RID rid(key);
    Insert(index_key, rid, transaction);
  }
}
/*
 * This method is used for test only
 * Read data from file and remove one by one
 */
INDEX_TEMPLATE_ARGUMENTS
void BPLUSTREE_TYPE::RemoveFromFile(const std::string &file_name, Transaction *transaction) {
  int64_t key;
  std::ifstream input(file_name);
  while (input) {
    input >> key;
    KeyType index_key;
    index_key.SetFromInteger(key);
    Remove(index_key, transaction);
  }
}

/**
 * This method is used for debug only, You don't need to modify
 */
INDEX_TEMPLATE_ARGUMENTS
void BPLUSTREE_TYPE::Draw(BufferPoolManager *bpm, const std::string &outf) {
  if (IsEmpty()) {
    LOG_WARN("Draw an empty tree");
    return;
  }
  std::ofstream out(outf);
  out << "digraph G {" << std::endl;
  ToGraph(reinterpret_cast<BPlusTreePage *>(bpm->FetchPage(root_page_id_)->GetData()), bpm, out);
  out << "}" << std::endl;
  out.flush();
  out.close();
}

/**
 * This method is used for debug only, You don't need to modify
 */
INDEX_TEMPLATE_ARGUMENTS
void BPLUSTREE_TYPE::Print(BufferPoolManager *bpm) {
  if (IsEmpty()) {
    LOG_WARN("Print an empty tree");
    return;
  }
  ToString(reinterpret_cast<BPlusTreePage *>(bpm->FetchPage(root_page_id_)->GetData()), bpm);
}

/**
 * This method is used for debug only, You don't need to modify
 * @tparam KeyType
 * @tparam ValueType
 * @tparam KeyComparator
 * @param page
 * @param bpm
 * @param out
 */
INDEX_TEMPLATE_ARGUMENTS
void BPLUSTREE_TYPE::ToGraph(BPlusTreePage *page, BufferPoolManager *bpm, std::ofstream &out) const {
  std::string leaf_prefix("LEAF_");
  std::string internal_prefix("INT_");
  if (page->IsLeafPage()) {
    auto *leaf = reinterpret_cast<LeafPage *>(page);
    // Print node name
    out << leaf_prefix << leaf->GetPageId();
    // Print node properties
    out << "[shape=plain color=green ";
    // Print data of the node
    out << "label=<<TABLE BORDER=\"0\" CELLBORDER=\"1\" CELLSPACING=\"0\" CELLPADDING=\"4\">\n";
    // Print data
    out << "<TR><TD COLSPAN=\"" << leaf->GetSize() << "\">P=" << leaf->GetPageId() << "</TD></TR>\n";
    out << "<TR><TD COLSPAN=\"" << leaf->GetSize() << "\">"
        << "max_size=" << leaf->GetMaxSize() << ",min_size=" << leaf->GetMinSize() << ",size=" << leaf->GetSize()
        << "</TD></TR>\n";
    out << "<TR>";
    for (int i = 0; i < leaf->GetSize(); i++) {
      out << "<TD>" << leaf->KeyAt(i) << "</TD>\n";
    }
    out << "</TR>";
    // Print table end
    out << "</TABLE>>];\n";
    // Print Leaf node link if there is a next page
    if (leaf->GetNextPageId() != INVALID_PAGE_ID) {
      out << leaf_prefix << leaf->GetPageId() << " -> " << leaf_prefix << leaf->GetNextPageId() << ";\n";
      out << "{rank=same " << leaf_prefix << leaf->GetPageId() << " " << leaf_prefix << leaf->GetNextPageId() << "};\n";
    }

    // Print parent links if there is a parent
    if (leaf->GetParentPageId() != INVALID_PAGE_ID) {
      out << internal_prefix << leaf->GetParentPageId() << ":p" << leaf->GetPageId() << " -> " << leaf_prefix
          << leaf->GetPageId() << ";\n";
    }
  } else {
    auto *inner = reinterpret_cast<InternalPage *>(page);
    // Print node name
    out << internal_prefix << inner->GetPageId();
    // Print node properties
    out << "[shape=plain color=pink ";  // why not?
    // Print data of the node
    out << "label=<<TABLE BORDER=\"0\" CELLBORDER=\"1\" CELLSPACING=\"0\" CELLPADDING=\"4\">\n";
    // Print data
    out << "<TR><TD COLSPAN=\"" << inner->GetSize() << "\">P=" << inner->GetPageId() << "</TD></TR>\n";
    out << "<TR><TD COLSPAN=\"" << inner->GetSize() << "\">"
        << "max_size=" << inner->GetMaxSize() << ",min_size=" << inner->GetMinSize() << ",size=" << inner->GetSize()
        << "</TD></TR>\n";
    out << "<TR>";
    for (int i = 0; i < inner->GetSize(); i++) {
      out << "<TD PORT=\"p" << inner->ValueAt(i) << "\">";
      if (i > 0) {
        out << inner->KeyAt(i);
      } else {
        out << " ";
      }
      out << "</TD>\n";
    }
    out << "</TR>";
    // Print table end
    out << "</TABLE>>];\n";
    // Print Parent link
    if (inner->GetParentPageId() != INVALID_PAGE_ID) {
      out << internal_prefix << inner->GetParentPageId() << ":p" << inner->GetPageId() << " -> " << internal_prefix
          << inner->GetPageId() << ";\n";
    }
    // Print leaves
    for (int i = 0; i < inner->GetSize(); i++) {
      auto child_page = reinterpret_cast<BPlusTreePage *>(bpm->FetchPage(inner->ValueAt(i))->GetData());
      ToGraph(child_page, bpm, out);
      if (i > 0) {
        auto sibling_page = reinterpret_cast<BPlusTreePage *>(bpm->FetchPage(inner->ValueAt(i - 1))->GetData());
        if (!sibling_page->IsLeafPage() && !child_page->IsLeafPage()) {
          out << "{rank=same " << internal_prefix << sibling_page->GetPageId() << " " << internal_prefix
              << child_page->GetPageId() << "};\n";
        }
        bpm->UnpinPage(sibling_page->GetPageId(), false);
      }
    }
  }
  bpm->UnpinPage(page->GetPageId(), false);
}

/**
 * This function is for debug only, you don't need to modify
 * @tparam KeyType
 * @tparam ValueType
 * @tparam KeyComparator
 * @param page
 * @param bpm
 */
INDEX_TEMPLATE_ARGUMENTS
void BPLUSTREE_TYPE::ToString(BPlusTreePage *page, BufferPoolManager *bpm) const {
  if (page->IsLeafPage()) {
    auto *leaf = reinterpret_cast<LeafPage *>(page);
    std::cout << "Leaf Page: " << leaf->GetPageId() << " parent: " << leaf->GetParentPageId()
              << " next: " << leaf->GetNextPageId() << std::endl;
    for (int i = 0; i < leaf->GetSize(); i++) {
      std::cout << leaf->KeyAt(i) << ",";
    }
    std::cout << std::endl;
    std::cout << std::endl;
  } else {
    auto *internal = reinterpret_cast<InternalPage *>(page);
    std::cout << "Internal Page: " << internal->GetPageId() << " parent: " << internal->GetParentPageId() << std::endl;
    for (int i = 0; i < internal->GetSize(); i++) {
      std::cout << internal->KeyAt(i) << ": " << internal->ValueAt(i) << ",";
    }
    std::cout << std::endl;
    std::cout << std::endl;
    for (int i = 0; i < internal->GetSize(); i++) {
      ToString(reinterpret_cast<BPlusTreePage *>(bpm->FetchPage(internal->ValueAt(i))->GetData()), bpm);
    }
  }
  bpm->UnpinPage(page->GetPageId(), false);
}

template class BPlusTree<GenericKey<4>, RID, GenericComparator<4>>;
template class BPlusTree<GenericKey<8>, RID, GenericComparator<8>>;
template class BPlusTree<GenericKey<16>, RID, GenericComparator<16>>;
template class BPlusTree<GenericKey<32>, RID, GenericComparator<32>>;
template class BPlusTree<GenericKey<64>, RID, GenericComparator<64>>;

}  // namespace bustub
