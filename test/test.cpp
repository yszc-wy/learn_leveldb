#include <assert.h>
#include <iostream>
#include <sstream>
#include <leveldb/db.h>

using namespace std;

int main(){
  leveldb::DB* db;
  leveldb::Options options;
  options.create_if_missing=true;
  // 打开数据库
  leveldb::Status status=leveldb::DB::Open(options,"./testdb1",&db);
  int count=0;
  while(count<1000){
    string key="yszc-"+to_string(count);
    string value="yszc-wy@foxmail.com";

    status=db->Put(leveldb::WriteOptions(),key,value);
    assert(status.ok());

    status=db->Get(leveldb::ReadOptions(),key,&value);
    assert(status.ok());
    std::cout<<value<<std::endl;

    ++count;
  }

  delete db;
  return 0;
}
