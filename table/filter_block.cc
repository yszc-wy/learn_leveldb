// Copyright (c) 2012 The LevelDB Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file. See the AUTHORS file for names of contributors.

#include "table/filter_block.h"

#include "leveldb/filter_policy.h"
#include "util/coding.h"

namespace leveldb {

// See doc/table_format.md for an explanation of the filter block format.

// 每2kb数据生成新的filter和filter offset
// Generate new filter every 2KB of data
static const size_t kFilterBaseLg = 11;
static const size_t kFilterBase = 1 << kFilterBaseLg;

FilterBlockBuilder::FilterBlockBuilder(const FilterPolicy* policy)
    : policy_(policy) {}

// 如果我添加了一个block,该block offset计算后属于filter data 0
// 然后我继续添加一个block,如果该block offset计算后依然属于filter data0,此时由于没有进行GenerateFilter,filtersize为0,依然可以插入该block到还未清空的缓冲区
void FilterBlockBuilder::StartBlock(uint64_t block_offset) {
  // 给出data block的offset,判断该block在哪个filter上,可以用移位运算符确定
  uint64_t filter_index = (block_offset / kFilterBase);
  // block_offset必须位于还未添加的filter_index中
  assert(filter_index >= filter_offsets_.size());
  // 如果index不是最近未添加的filter index,也就是之前的filter还没有到写入result中,就写入filter data,如果中间空太多,就插入空的filter data
  while (filter_index > filter_offsets_.size()) {
    GenerateFilter();
  }
}

void FilterBlockBuilder::AddKey(const Slice& key) {
  // 填充状态缓冲
  Slice k = key;
  start_.push_back(keys_.size());
  keys_.append(k.data(), k.size());
}

Slice FilterBlockBuilder::Finish() {
  // 如果start不为空
  if (!start_.empty()) {
    GenerateFilter();
  }

  // Append array of per-filter offsets
  const uint32_t array_offset = result_.size();
  // 将filter offset插入到result后面
  for (size_t i = 0; i < filter_offsets_.size(); i++) {
    PutFixed32(&result_, filter_offsets_[i]);
  }

  // 在尾部保存filter offset数组的起始位置
  PutFixed32(&result_, array_offset);
  // 保存base的幂次 
  result_.push_back(kFilterBaseLg);  // Save encoding parameter in result
  return Slice(result_);
}

// GenerateFilter与blockoffset是解耦的,它只负责用缓冲区中的key生成新的filter data,并插入到result中
// 何时使用GenerateFilter由StartBlock对blockoffset的计算结果决定
// 根据start中缓存的key生成Filter data,如果缓存中没有key,就生成一个空的filter,最终插入到result中
void FilterBlockBuilder::GenerateFilter() {
  const size_t num_keys = start_.size();
  if (num_keys == 0) {
    // Fast path if there are no keys for this filter
    filter_offsets_.push_back(result_.size());
    return;
  }

  // Make list of keys from flattened key structure
  // 将keys_和starts中的内容写入到tmp_keys参数中
  start_.push_back(keys_.size());  // Simplify length computation
  tmp_keys_.resize(num_keys);
  for (size_t i = 0; i < num_keys; i++) {
    const char* base = keys_.data() + start_[i];
    size_t length = start_[i + 1] - start_[i];
    tmp_keys_[i] = Slice(base, length);
  }

  // Generate filter for current set of keys and append to result_.
  filter_offsets_.push_back(result_.size());
  policy_->CreateFilter(&tmp_keys_[0], static_cast<int>(num_keys), &result_);

  // 每生成一个Filter就清理一次状态
  tmp_keys_.clear();
  keys_.clear();
  start_.clear();
}

// 解析filter block
FilterBlockReader::FilterBlockReader(const FilterPolicy* policy,
                                     const Slice& contents)
    : policy_(policy), data_(nullptr), offset_(nullptr), num_(0), base_lg_(0) {
  size_t n = contents.size();
  // 最小长度
  if (n < 5) return;  // 1 byte for base_lg_ and 4 for start of offset array
  base_lg_ = contents[n - 1];
  // fillter offset数组的起始位置
  uint32_t last_word = DecodeFixed32(contents.data() + n - 5);
  if (last_word > n - 5) return;
  // 记录block头指针
  data_ = contents.data();
  // offset数组指针
  offset_ = data_ + last_word;
  // offset数组元素数量
  num_ = (n - 5 - last_word) / 4;
}

// 使用blockoffset和block内的key来快速判断该data block中key是否存在
bool FilterBlockReader::KeyMayMatch(uint64_t block_offset, const Slice& key) {
  // 通过offset确定index (移位运算符代替除法和取余)
  uint64_t index = block_offset >> base_lg_;
  if (index < num_) {
    uint32_t start = DecodeFixed32(offset_ + index * 4);
    uint32_t limit = DecodeFixed32(offset_ + index * 4 + 4);
    if (start <= limit && limit <= static_cast<size_t>(offset_ - data_)) {
      Slice filter = Slice(data_ + start, limit - start);
      return policy_->KeyMayMatch(key, filter);
    } else if (start == limit) {
      // Empty filters do not match any keys
      return false;
    }
  }
  return true;  // Errors are treated as potential matches
}

}  // namespace leveldb
