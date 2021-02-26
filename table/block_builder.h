// Copyright (c) 2011 The LevelDB Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file. See the AUTHORS file for names of contributors.

#ifndef STORAGE_LEVELDB_TABLE_BLOCK_BUILDER_H_
#define STORAGE_LEVELDB_TABLE_BLOCK_BUILDER_H_

#include <cstdint>
#include <vector>

#include "leveldb/slice.h"

namespace leveldb {

struct Options;

// 对block进行编码
class BlockBuilder {
 public:
  explicit BlockBuilder(const Options* options);

  BlockBuilder(const BlockBuilder&) = delete;
  BlockBuilder& operator=(const BlockBuilder&) = delete;

  // Reset the contents as if the BlockBuilder was just constructed.
  void Reset();

  // REQUIRES: Finish() has not been called since the last call to Reset().
  // REQUIRES: key is larger than any previously added key
  // Reset调用之后,Finish调用之前
  // key要比之前添加的key要大
  void Add(const Slice& key, const Slice& value);

  // 结束block构建,返回指向block content的slice,该slice的生命周期在调用该builder调用Reset之前
  // Finish building the block and return a slice that refers to the
  // block contents.  The returned slice will remain valid for the
  // lifetime of this builder or until Reset() is called.
  // yszc:由于block在add未结束时处于不确定的状态,并不会在每一次add就返回一个slice,而是知道用户添加完元素后,统一编码
  Slice Finish();

  // 返回block大小估计
  // Returns an estimate of the current (uncompressed) size of the block
  // we are building.
  size_t CurrentSizeEstimate() const;

  // Return true iff no entries have been added since the last Reset()
  // 清空编吗区
  bool empty() const { return buffer_.empty(); }

 private:
  const Options* options_;
  // yszc: buffer缓存添加的entry!!!
  std::string buffer_;              // Destination buffer
  std::vector<uint32_t> restarts_;  // Restart points
  // 重启点间隔计数器
  int counter_;                     // Number of entries emitted since restart
  bool finished_;                   // Has Finish() been called?
  std::string last_key_;
};

}  // namespace leveldb

#endif  // STORAGE_LEVELDB_TABLE_BLOCK_BUILDER_H_
