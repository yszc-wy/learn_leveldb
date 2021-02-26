// Copyright (c) 2011 The LevelDB Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file. See the AUTHORS file for names of contributors.

#ifndef STORAGE_LEVELDB_DB_BUILDER_H_
#define STORAGE_LEVELDB_DB_BUILDER_H_

#include "leveldb/status.h"

namespace leveldb {

struct Options;
struct FileMetaData;

class Env;
class Iterator;
class TableCache;
class VersionEdit;

// 使用iter构造table文件,并根据meta->number命名,若成果,meta会被填充
// Build a Table file from the contents of *iter.  The generated file
// will be named according to meta->number.  On success, the rest of
// *meta will be filled with metadata about the generated table.
// If no data is present in *iter, meta->file_size will be set to
// zero, and no Table file will be produced.
// 构造好的table会放入缓存中
Status BuildTable(const std::string& dbname, Env* env, const Options& options,
                  TableCache* table_cache, Iterator* iter, FileMetaData* meta);

}  // namespace leveldb

#endif  // STORAGE_LEVELDB_DB_BUILDER_H_
