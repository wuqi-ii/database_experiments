//===----------------------------------------------------------------------===//
//                         BusTub
// buffer_pool_manager_edge_test.cpp
// Copyright (c) 2015-2022, Carnegie Mellon University Database Group
//===----------------------------------------------------------------------===//

#include <cstdio>
#include <cstring>

#include "buffer/buffer_pool_manager_instance.h"
#include "gtest/gtest.h"

namespace bustub {

TEST(BufferPoolManagerEdgeTest, FlushAllIncludesCleanPinnedPagesAndClearsDirtyFlags) {
  {
    DiskManager disk("bpm_flush_edge.db");
    BufferPoolManagerInstance manager(3, &disk, 2);
    manager.FlushAllPages();
    EXPECT_EQ(disk.GetNumWrites(), 0);

    page_id_t clean_id = INVALID_PAGE_ID;
    page_id_t dirty_id = INVALID_PAGE_ID;
    auto *clean_page = manager.NewPage(&clean_id);
    auto *dirty_page = manager.NewPage(&dirty_id);
    ASSERT_NE(clean_page, nullptr);
    ASSERT_NE(dirty_page, nullptr);
    std::memcpy(clean_page->GetData(), "clean", 6);
    std::memcpy(dirty_page->GetData(), "dirty", 6);
    ASSERT_TRUE(manager.UnpinPage(dirty_id, true));

    manager.FlushAllPages();
    EXPECT_EQ(disk.GetNumWrites(), 2);
    EXPECT_FALSE(clean_page->IsDirty());
    EXPECT_FALSE(dirty_page->IsDirty());
    EXPECT_EQ(clean_page->GetPinCount(), 1);
    EXPECT_EQ(dirty_page->GetPinCount(), 0);
    char data[BUSTUB_PAGE_SIZE]{};
    disk.ReadPage(clean_id, data);
    EXPECT_STREQ(data, "clean");
    disk.ReadPage(dirty_id, data);
    EXPECT_STREQ(data, "dirty");

    ASSERT_NE(manager.FetchPage(dirty_id), nullptr);
    ASSERT_TRUE(manager.UnpinPage(dirty_id, true));
    ASSERT_TRUE(manager.FlushPage(dirty_id));
    EXPECT_FALSE(dirty_page->IsDirty());
    std::memcpy(clean_page->GetData(), "again", 6);
    ASSERT_TRUE(manager.FlushPage(clean_id));
    disk.ReadPage(clean_id, data);
    EXPECT_STREQ(data, "again");
    EXPECT_EQ(disk.GetNumWrites(), 4);
    EXPECT_FALSE(manager.FlushPage(INVALID_PAGE_ID));
    EXPECT_EQ(disk.GetNumWrites(), 4);
    disk.ShutDown();
  }
  std::remove("bpm_flush_edge.db");
  std::remove("bpm_flush_edge.log");
}

TEST(BufferPoolManagerEdgeTest, FailedAllocationAndFrameReusePreserveState) {
  {
    DiskManager disk("bpm_reuse_edge.db");
    BufferPoolManagerInstance manager(1, &disk, 2);
    page_id_t first_id = INVALID_PAGE_ID;
    auto *page = manager.NewPage(&first_id);
    ASSERT_NE(page, nullptr);
    std::memcpy(page->GetData(), "saved", 6);
    page_id_t failed_id = -123;
    EXPECT_EQ(manager.NewPage(&failed_id), nullptr);
    EXPECT_EQ(failed_id, -123);
    ASSERT_TRUE(manager.UnpinPage(first_id, true));
    ASSERT_NE(manager.FetchPage(first_id), nullptr);
    ASSERT_TRUE(manager.UnpinPage(first_id, false));
    EXPECT_TRUE(page->IsDirty());

    page_id_t second_id = INVALID_PAGE_ID;
    page = manager.NewPage(&second_id);
    ASSERT_NE(page, nullptr);
    EXPECT_EQ(second_id, first_id + 1);
    EXPECT_EQ(page->GetData()[0], '\0');
    EXPECT_FALSE(page->IsDirty());
    EXPECT_EQ(page->GetPinCount(), 1);
    ASSERT_TRUE(manager.UnpinPage(second_id, false));
    page = manager.FetchPage(first_id);
    ASSERT_NE(page, nullptr);
    EXPECT_STREQ(page->GetData(), "saved");
    ASSERT_TRUE(manager.UnpinPage(first_id, false));
    ASSERT_TRUE(manager.DeletePage(first_id));
    EXPECT_EQ(page->GetPageId(), INVALID_PAGE_ID);
    EXPECT_EQ(page->GetPinCount(), 0);
    EXPECT_FALSE(page->IsDirty());
    EXPECT_EQ(page->GetData()[0], '\0');
    EXPECT_EQ(manager.FetchPage(INVALID_PAGE_ID), nullptr);
    disk.ShutDown();
  }
  std::remove("bpm_reuse_edge.db");
  std::remove("bpm_reuse_edge.log");
}

}  // namespace bustub
