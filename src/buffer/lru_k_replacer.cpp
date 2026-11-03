//===----------------------------------------------------------------------===//
//
//                         BusTub
//
// lru_k_replacer.cpp
//
// Identification: src/buffer/lru_k_replacer.cpp
//
// Copyright (c) 2015-2022, Carnegie Mellon University Database Group
//
//===----------------------------------------------------------------------===//

#include "buffer/lru_k_replacer.h"
#include <stdexcept>

namespace bustub {

LRUKReplacer::LRUKReplacer(size_t num_frames, size_t k) : replacer_size_(num_frames), k_(k) {}

void LRUKReplacer::RecordAccess(frame_id_t frame_id) {
  std::scoped_lock lock(latch_);
  if (frame_id < 0 || static_cast<size_t>(frame_id) >= replacer_size_) {
    throw std::out_of_range("invalid frame id");
  }

  auto &history = nodes_[frame_id].history_;
  history.push_back(current_timestamp_++);
  if (history.size() > k_) {
    history.pop_front();
  }
}

void LRUKReplacer::SetEvictable(frame_id_t frame_id, bool set_evictable) {
  std::scoped_lock lock(latch_);
  if (frame_id < 0 || static_cast<size_t>(frame_id) >= replacer_size_) {
    throw std::out_of_range("invalid frame id");
  }

  auto it = nodes_.find(frame_id);
  if (it == nodes_.end() || it->second.evictable_ == set_evictable) {
    return;
  }

  it->second.evictable_ = set_evictable;
  if (set_evictable) {
    ++curr_size_;
  } else {
    --curr_size_;
  }
}

auto LRUKReplacer::Size() -> size_t {
  std::scoped_lock lock(latch_);
  return curr_size_;
}

auto LRUKReplacer::Evict(frame_id_t *frame_id) -> bool {
  std::scoped_lock lock(latch_);
  auto victim = nodes_.end();

  for (auto it = nodes_.begin(); it != nodes_.end(); ++it) {
    const auto &node = it->second;
    if (!node.evictable_) {
      continue;
    }
    if (victim == nodes_.end()) {
      victim = it;
      continue;
    }

    const auto &best = victim->second;
    bool under_k = node.history_.size() < k_;
    bool best_under_k = best.history_.size() < k_;

    // 访问不足 K 次的帧优先淘汰。同组内选历史中最早时间戳更小的帧。
    if ((under_k && !best_under_k) ||
        (under_k == best_under_k && node.history_.front() < best.history_.front())) {
      victim = it;
    }
  }

  if (victim == nodes_.end()) {
    return false;
  }

  *frame_id = victim->first;
  nodes_.erase(victim);
  --curr_size_;
  return true;
}

void LRUKReplacer::Remove(frame_id_t frame_id) {
  std::scoped_lock lock(latch_);
  auto it = nodes_.find(frame_id);
  if (it == nodes_.end()) {
    return;
  }
  if (!it->second.evictable_) {
    throw std::logic_error("cannot remove a non-evictable frame");
  }

  nodes_.erase(it);
  --curr_size_;
}

}  // namespace bustub
