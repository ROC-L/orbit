// Copyright (c) 2020 The Orbit Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ORBIT_GL_TIMER_CHAIN_
#define ORBIT_GL_TIMER_CHAIN_

#include <algorithm>
#include <atomic>
#include <cstdint>
#include <iosfwd>
#include <limits>

#include "TextBox.h"

static constexpr int kBlockSize = 1024;
class TimerChain;

// TimerBlock is a straightforward specialization of Block (see BlockChain.h)
// with the added bonus that it keeps track of the minimum and maximum
// timestamps of all timers added to it. This allows trivial rejection of an
// entire block by using the Intersects(t_min, t_max) method. This effectively
// tests if any of the timers stored in this block intersects with the [t_min,
// t_max] interval.
class TimerBlock {
  friend class TimerChain;
  friend class TimerChainIterator;

 public:
  TimerBlock(TimerChain* chain, TimerBlock* prev)
      : prev_(prev),
        next_(nullptr),
        chain_(chain),
        size_(0),
        min_timestamp_(std::numeric_limits<uint64_t>::max()),
        max_timestamp_(std::numeric_limits<uint64_t>::min()) {}

  // Adds an item to the block. If capacity of this block is reached, a new
  // blocked is allocated and the item is added to the new block.
  void Add(const TextBox& item);

  // Tests if [min, max] intersects with [min_timestamp, max_timestamp], where
  // {min, max}_timestamp are the minimum and maximum timestamp of the timers
  // that have so far been added to this block.
  bool Intersects(uint64_t min, uint64_t max) const;

  uint64_t size() const { return size_; }

  TextBox& operator[](std::size_t idx) { return data_[idx]; }

  const TextBox& operator[](std::size_t idx) const { return data_[idx]; }

 private:
  TimerBlock* prev_;
  TimerBlock* next_;
  TimerChain* chain_;
  uint64_t size_;
  TextBox data_[kBlockSize];

  uint64_t min_timestamp_;
  uint64_t max_timestamp_;
};

// TimerChainIterator iterates over all *blocks* of the chain, not the
// individual items (TextBox instances) that are stored in the blocks (this is
// different from the BlockIterator in BlockChain.h).
class TimerChainIterator {
 public:
  explicit TimerChainIterator(TimerBlock* block) : block_(block) {}
  TimerChainIterator& operator=(const TimerChainIterator& other) = default;
  TimerChainIterator(const TimerChainIterator& other) = default;
  TimerChainIterator(TimerChainIterator&& other) = default;
  TimerChainIterator& operator=(TimerChainIterator&& other) = default;

  bool operator!=(const TimerChainIterator& other) const { return !(*this == other); }

  bool operator==(const TimerChainIterator& other) const { return block_ == other.block_; }
  TimerChainIterator& operator++() {
    block_ = block_->next_;
    return *this;
  }

  const TimerBlock& operator*() const { return *block_; }
  TimerBlock& operator*() { return *block_; }

  TimerBlock* operator->() { return block_; }
  const TimerBlock* operator->() const { return block_; }

 private:
  TimerBlock* block_;
};

// TimerChain is a specialization of the BlockChain data structure to make it
// easier to keep track of min and max timestamps in the blocks, which allows
// for fast rejection of entire blocks when rendering timers. Note that there
// is a difference compared with BlockChain in how the iterators work: Here,
// the iterator runs over blocks, in BlockChain the iterator runs over the
// individually stored elements.
class TimerChain {
  friend class TimerBlock;

 public:
  TimerChain() : num_blocks_(1), num_items_(0) { root_ = current_ = new TimerBlock(this, nullptr); }

  ~TimerChain();

  void push_back(const TextBox& item) { current_->Add(item); }
  [[nodiscard]] bool empty() const { return num_items_ == 0; }
  [[nodiscard]] uint64_t size() const { return num_items_; }

  [[nodiscard]] TimerBlock* GetBlockContaining(const TextBox* element) const;

  [[nodiscard]] TextBox* GetElementAfter(const TextBox* element) const;

  [[nodiscard]] TextBox* GetElementBefore(const TextBox* element) const;

  [[nodiscard]] TimerChainIterator begin() { return TimerChainIterator(root_); }

  [[nodiscard]] TimerChainIterator end() { return TimerChainIterator(nullptr); }

 private:
  TimerBlock* root_;
  TimerBlock* current_;
  uint64_t num_blocks_;
  uint64_t num_items_;
};

#endif
