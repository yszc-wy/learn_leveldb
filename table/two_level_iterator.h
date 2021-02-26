// Copyright (c) 2011 The LevelDB Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file. See the AUTHORS file for names of contributors.

#ifndef STORAGE_LEVELDB_TABLE_TWO_LEVEL_ITERATOR_H_
#define STORAGE_LEVELDB_TABLE_TWO_LEVEL_ITERATOR_H_

#include "leveldb/iterator.h"

namespace leveldb {

struct ReadOptions;

// index iterator 包含指向blocks序列的值
// Return a new two level iterator.  A two-level iterator contains an
// index iterator whose values point to a sequence of blocks where
// each block is itself a sequence of key,value pairs.  The returned
// two-level iterator yields the concatenation of all key/value pairs
// in the sequence of blocks.  Takes ownership of "index_iter" and
// will delete it when no longer needed.
//
// 使用block_function将一个index_iterator中包含的值转化为对应的block迭代器
// Uses a supplied function to convert an index_iter value into
// an iterator over the contents of the corresponding block.
// 最终实现的2阶段迭代器可以在这一系列的block中遍历每个entry
// 最后2个参数是blockfunction的前2个参数,最后一个是index_iter保存的index_value
// index_iter是一个table中的index迭代器(也是block迭代器)
// 通过index迭代获取data block的位置
Iterator* NewTwoLevelIterator(
    // 如果是tableindex
    Iterator* index_iter,
    // 这里就是readblock函数
    Iterator* (*block_function)(void* arg, const ReadOptions& options,
                                const Slice& index_value),
    void* arg, const ReadOptions& options);

}  // namespace leveldb

#endif  // STORAGE_LEVELDB_TABLE_TWO_LEVEL_ITERATOR_H_
