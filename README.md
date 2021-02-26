# 源码笔记


- 线程安全性分析: [https://clang.llvm.org/docs/ThreadSafetyAnalysis.html](https://clang.llvm.org/docs/ThreadSafetyAnalysis.html)

- emplace_back可以避免使用参数构造对象时额外的构造操作,直接原地通过参数构造对象,如果是push_back,需要调用构造函数构造临时对象,再调用移动构造函数构造对象.

- PosixWritableFile使用写缓冲区，写缓冲区积累写入操作，直到缓冲区满才进行一次批处理写入，减少写操作的调用次数，如果写入数据过大就才循环写入。


- leveldb arena [https://www.jianshu.com/p/f5eebf44dec9](https://www.jianshu.com/p/f5eebf44dec9)
  - leveldb arena减少了内存malloc的次数,小的内存申请一般分配一次block可以用很长时间,剩余空间不够就丢弃剩余空间再重新分配一个block.所以为了减少block剩余空间的浪费,将过大的内存申请单独分配空间(和缓冲写文件的思想有点类似,减少了syscall调用的次数）
  - 在LevelDB里面为了防止skiplist大量多次内存分配导致的碎片，采用了自己写的一套内存分配管理器。名称为Arena。
  - 但是需要注意的是，这个内存分配器并不是为整个LevelDB项目考虑的。主要是为skiplist也就是memtable服务。
  - skiplist里面记录的是用户传进来的key/value，这些字符串有长有短，放到内存中的时候，很容易导致内存碎片。所以这里写了一个统一的内存管理器。
  - skiplist/memtable要申请内存的时候，就利用Arena分配器来分配内存。当skiplist/memtable要释放的时候，就直接通过Arena类的block_把所有申请的内存释放掉。
  - align&align-1==0保证align是2的冥次方!!!
  - reinterpret_cast<uintptr_t>(alloc_ptr_) & (align - 1); 快速取余操作
  - arena还提供了allocateAligned,方便从block中返回对齐的分配地址
    
- leveldb cache提供了对entry的LRU缓冲,且有容量限制不用担心内存的过度占用
  - LRUCache 链表查找返回的是指针的指针
  - LRUHandle的ref ,ref函数和unref函数
      - 在从LRU中删除指定的entry时,如果entry在lru表中,ref=1,则LRU在unref时负责析构value(无人使用),若在in_use表中unref就不会析构,因为还在使用,这是引用计数的好处.
  - LRUCache中ref和in_use_链表,lru_链表的结合
  - ShardedLRUCache的分片机制增加了并发度，分片机制的大部分函数都转而执行指定分片的lrucache的函数，不需要并发保护
  - HashTable中的FindPointer使用LRUHandle**保证了返回值一定是有效的，不需要对LRHandle*为空的情况进行特殊处理,而且插入和删除非常方便
  - HashTable不是线程安全的，cache拥有一个mutex_来保障线程安全
  - LRUCache 由lru_链表,in_use链表,一个hash_table组成,LRUHandle是上述3个数据结构的成员,LRUHandle拥有3个指针,2个用于双向链表,一个用于hash_table中的散列链表,leveldb实现了专用于LRUCache和LRUhandle的hash table以提高效率,我们通过hash_table来提供对key_value的O(1)访问,通过lru_双链表执行lru缓存策略,通过in_use链表保护正在被用户使用的handle
  - 我们通过LRUHandle的引用计数来管理LRUHandle所处链表的位置,当用户insert key,value entry时,LRUCache创建Handle,将其保存到in_use链表并返回给用户,这时in_use链表持有一个对该handle的引用计数,用户也可能同时持有多个ref,这时handle的ref>=2,如果用户不再使用该handle,并使用unref降低ref,当ref降低到1时,说明用户不再持有该handle,该handle被移动到lru_链表中,所以lru_链表中的所有handle的ref都为1,同理当ref增加到2时,还会被移动回in_use链表中,lru_中的链表可以根据其capacity进行相应的淘汰,当lru_的size已满,会淘汰最旧的Handle并释放Handle

- coding.cc
    - varint32为了表示变长的32位数字,需要在位数不固定的情况下通过字节开头的标志位来确定下一个字节是否也是数字的一部分,所以使用了比较麻烦的数字表示法 ([https://www.cnblogs.com/duma/p/11111427.html](https://www.cnblogs.com/duma/p/11111427.html))
    - 将每个字节的最高位作为标志位,剩下的其他位用于存储数值
    - 这会导致小的32位数最小只要一个byte,大的32位数最多要5Byte (有点概率启发的感觉,就是说小数出现的频率比大数高)
    - 可以看出varint32以小端形式存储，低位放低地址，高位放高地址
    - 注意编码端口EncodeVarint32返回的是末尾下一个指针，方便继续编码
    
- env系列
    - env.h是和系统相关调用的抽象接口,若要移植到其他系统,应该实现这些接口
    - env_posix提供了posix环境下相关文件读写,文件锁,自定义资源限制,线程启动,任务调度等的实现
    - memenv.cc将env接口中文件系统相关的接口实现为基于内存的虚拟文件读写
    
- filter_policy系列
    - filter_policy.h提供了过滤器的接口,负责生成一组有序key的summary,并可以快速判断出summary是否有指定的key,用于磁盘块过滤,可以多磁盘查找锐减为单磁盘查找
    - bloom.cc bloom过滤器实现
    
- hash.h
    - 使用了著名的murmurHash算法
    
- leveldb 文件格式 ([https://zhuanlan.zhihu.com/p/149794494](https://zhuanlan.zhihu.com/p/149794494))

```
<beginning_of_file>
[data block 1]
[data block 2]
...
[data block N]
[meta block 1]
...
[meta block K]
[metaindex block]
[index block]
[Footer]        (fixed size; starts at file_size - sizeof(Footer))
<end_of_file>

```

- 文件包含内部指针,每一个内部指针称为BlockHandle,并包含如下信息:

```
offset:   varint64
size:     varint64
```

- 文件中的key/value pair序列以排序的顺序存储，并划分为一系列data block。这些块在文件的开头一个接一个地出现。我们根据block_builder.cc中的代码格式化每个数据块，然后可选地进行压缩。
- 一个data block中包含多个key/value pair
- 在数据块之后，我们存储了一堆meta block。支持的元数据块类型如下所述。将来可能会添加更多的元块类型。再次使用block_builder.cc格式化每个元块，然后可选地进行压缩。
- metaindex block 它为每一个meta block包含一个entry，该entry的key是meta block的名称，值是指向该meta block的BlockHandle 实际上是一个索引,通过访问和遍历metaindex block快速定位其他meta block在文件中的位置
- index block为每个data block包含一个entry，entry的key是一个string,该string大于等于当前data block中的最后一个key，且小于下一个data block的第一个key。value是当前data block的BlockHandle index block是data block的索引,读取index block帮助我们根据指定的key快速定位到对应data block的位置
- 文件的末尾是 **固定长度**的页脚(Footer)，其中包含metaindex block handle、index block handle ,以及0填充(以此保持固定长度)和一个magic number(标记文件或者协议的格式)

```
 metaindex_handle: char[p];     // Block handle for metaindex
 index_handle:     char[q];     // Block handle for index
 padding:          char[40-p-q];// zeroed bytes to make fixed length
                                // (40==2*BlockHandle::kMaxEncodedLength)
 magic:            fixed64;     // == 0xdb4775248b80fb57 (little-endian)

```

- filter meta block
- 如果在打开数据库时指定了FilterPolicy，则 **一个**filter meta block存储在每个table中。
- 在metaindex中会包含 **一个**entry，该entry的key是"filter.<N>",value是filter meta block的BlockHandle，其中<N>是FilterPolicy的Name方法的返回值.
- filter meta block中存储了一个filters序列, **每一个filter i包含了文件偏移在指定范围内的data block中的所有key的summary**,summary由FilterPolicy::CreateFilter生成,指定的范围如下:

```
[ i*base ... (i+1)*base-1 ]

```

- 若base为2kb,说明一个filter可以负责大约2kb文件范围内的data block中的所有key的summary,对于filter0,其负责的文件范围是[0kb,2kb-1],filter1负责的文件范围是[2kb,4kb-1],以此类推

- 具体来说,如果blocks X和Y的起始位置都在[0kb,2kb-1]的文件范围内,则X和Y中的所有key都会被CreateFilter转化为summary保存在filter0的位置,也就是第一个filter的位置
- filter meta block的结构如下

```
[filter 0]
[filter 1]
[filter 2]
...
[filter N-1]
[offset of filter 0]                  : 4 bytes
[offset of filter 1]                  : 4 bytes
[offset of filter 2]                  : 4 bytes
...
[offset of filter N-1]                : 4 bytes
[offset of beginning of offset array] : 4 bytes // 指向 [offset of filter0],方便快速定位每个filter的位置
lg(base)                              : 1 byte

```

- stats meta block

- 包含一些统计信息

```
  data size
  index size
  key size (uncompressed)
  value size (uncompressed)
  number of entries
  number of data blocks
```

- format.h 为table文件中的每一个区域定义一个编码解码器(EncodeTo,DecodeFrom),为了编程的方便,EncodeTo一般对dst采用append模式,DecodeFrom对Slice会采用步进跳过已经解码的段
- leetcode 类的命名规范:对于access型的函数(get,set),小写+下划线,对于该类的功能函数,采用大写字母开头的写法
- Footer::DecodeFrom 使用幻数检查文件格式
- 每个block的尾部有1byte的类型码和32bit的校验码,BlockHandle只指向BlockContent的部分,不包含type和bit这4个字节的内容
- [format.cc](http://format.cc/) ReadBlock 负责根据指定的blockhandle读取block,并按照option对block进行校验或解压缩
    - BlockContent有2个标识, heap_allocated表示data是新分配出来的,需要在恰当的时候delete(如果BlockContent的data指针是复制来的而不是new出来的就不能带这个标志,否则可能出现2次delete),cachable表示data是可以被缓存的,如果该已经被缓存(比如被文件系统的实现所缓存),就不要缓存2次,cachable=false
    - BlockContent似乎是Block和ReadBlock接口的中间传递者,BlockContent包含了ReadBlock函数对其data数据空间管理上的描述,而Block根据其heap_allocated描述来决定对其data的生命周期管理策略,其出现似乎是为了简化读取block时出现的复杂情况, **可以让Block类与ReadBlock接口解耦**,...
- leveldb编解码器的设计
    - 对于Block中的简单结构：Blockhandle和Footer，都是一个对象直接负责解码和编码，其数据由成员对象保存
    - 当数据结构稍微大一点变成数据集合时，比如对于Block以及block以上的table结构
        - **解码操作时直接用迭代器，在迭代器中使用成员变量保存部分少量元数据**，大部分结果通过seek和next解码获得
        - 编码时也不需要也不可能从成员变量中编码，而是提供add函数，让用户添加编码内容
        - Block的编解码之间并无直接关系,因为block本身无需修改操作（sstable是不可变的，在sstable中我们只会读取旧的不可变数据，生成也只生成全新的数据，新旧数据之间没有直接的联系），所以我们这里将编码（BlockBuilder）和解码分开解耦设计是合理的。
        - table.h和table_builder也同理
        - 同时Block解码器还负责管理读取Block时分配的char数组的生命周期(owend=true的情况下)
    - leveldb中所有的decode操作全部都是步进的，方便后续编码
- index block、meta index block、data block 都是通过 BlockBuilder 来生成，通过 Block 来读取的。他们都用于保存key-value entry,最简单的方式，block 里面只需要将一个个 key-value 有序保存。但是为了节省空间，LevelDB 在 block 的内部实现了 **前缀压缩**(这个实现困难吗?会不会影响效率?)。
- 前缀压缩利用了 key 的有序性（前缀相同的有序 key 会聚集在一起）对 key 进行压缩，每个 key 与前一个 key 相同的前缀部分可以不用保存。读取的时候再根据规则进行解码即可。
- 前缀压缩使得包含重复前缀的key使用更少的空间，代价是某些操作可能更慢。因为每个值的压缩前缀都依赖前面的值. 细节见下:
- LevelDB 将 block 的一个 key-value 称为一条 entry。每条 entry 的格式如下：

shared_bytes:  |   unshared_bytes:  |   value_length:  |       key_delta:        |    value:
varint32     |      varint32      |      varint32    |   char[unshared_bytes]  |   char[value_length]

- shared_bytes：和 **前一个 key** 相同的前缀长度。
- unshared_bytes：和前一个 key不同的后缀部分的长度。
- value_length：value 数据的长度。
- key_delta：和前一个 key不同的后缀部分。
- value：value 数据。
- 可以看出,**定长字段在前,变长数据在后**
- 一个block的数据格式如下:

```
[entry0] restart point-0
...
[entry16] restart point-1
...
...
[restart[0]] uint32
[restart[1]] uint32
...
[restarts[num_restarts-1]] uint32
[num_restarts] uint32 // restart数组长度

```

- restarts：在 LevelDB 中，默认每 16 个 key 就会重新计算前缀压缩，重新开始计算前缀压缩到第一个 key 称之为重启点（restart point）。restarts 数组记录了这个 block 中所有重启点的 offset。
- **每一个重启点的entry的key都是完整的,也就是shared_bytes强制为0,包含entry的完整key,所有对于非重启点以外其他entry的访问必须在之前从该entry前面的重启点开始遍历,这样才能获取tar_entry的key的全部信息** 这引导了以下常规操作的逻辑:
    - 对于Block中entry的顺序访问,我们只需要从第一个重启点开始用ParseNextKey按顺序逐步解析entry即可

        ```
        // BlockIter ParseNextKey的key_解码逻辑
            // 截断到shared的位置(当遇到重启点时,该shared为0,正好完成了对key_的clear!!!!)
            key_.resize(shared);
            // 然后将解析出来的non_shared append进来(遇到重启点时,key_变成完整的key)
            key_.append(p, non_shared);

        ```

    - **对于倒序访问,必须先获得entry的上一个entry的最近restart point,然后从restart point开始线性解码到该entry,可以看出倒序有点低效率,每一次对entry的解析都要重新从restart point开始**
    - 对于随机的定位请求, **我们通过对restart数组进行二分搜索,先定位key<tar的重启点位置,然后从重启点开始顺序访问到tar entry**
- 总结: **restart point的出现就是为了优化前缀压缩无法使用二分查找的缺点**, **如果没有restart point,所有的随机访问尤其是倒叙访问都需要从block的头部开始查找**才能获取key的完整表示
    - restart_interval间隔为1,即无前缀压缩,restart array指向每一个entry
- 在 block 中查找一个 key（Block::Iter::Seek）：
    - 先在 restarts 数组的基础上进行二分查找，确定 restart point。
    - 从 restart point 开始遍历查找。
- Block接受BlockContent来进行初始化, **使用heap_allocated来决定是否该Block是否owned data**,owned data的Block会在析构函数中释放该data(负责生命周期的管理)

    - 可以看出,除非left一开始就在tar上,否则按照left=mid,left永远不会取到tar,因为只有A[mid]<tar时,left才会被赋值
    
- [Block.cc](http://block.cc/) BlockIter实现了在Block中遍历元素的接口,其实现原理见之前对restart point的解释,其seek函数将定位到等于或大于tar的第一个key位置,也就是tar可能存在的block的entry
- BlockIter对Iterator的实现体现了一种很好的抽象思维, **无论block内部的遍历多么复杂,我都可以使用iterator屏蔽底层的细节,为自己和用户提供简单方便的entry访问方法**
- table文件夹中的编码解码器的设计,简单的数据直接在一个类中集成编码解码的功能,复杂的数据,如block,使用block_builder类进行编码,使用block类返回的blockiter进行解码,block本身负责保存blockcontent(从文件中读取出来的)

block_builder.cc 指导了包含多个同类数据元素的data的编码. 使用了Reset重置编码器状态,Add添加entry,先将entry编码到缓存中,当用户添加完足够的entry之后,再由Finish将添加的元素连带restart array一起编码.该编码器是有状态的(不在直接实现编码返回slice可以add的效率)

- Add函数后面的状态更新是一个很好的注释,有状态的类都可以使用状态更新来提醒自己

filter_builder.cc 构建了特殊的filter_block,其调用接口的顺序遵循正则表达式 (StartBlock AddKey*)* Finish , 基本的调用是:

- StartBlock(blockoffset1) AddKey(key1) Addkey(key2) … StartBlock(blockoffset2) AddKey(key1) Addkey(key2) …. Finish()
    - 添加了2个block的key,也就是每一次startblock都将
- Addkey负责填充缓冲数据
- GenerateFilter和StartBlock是最重要的2个函数, **GenerateFilter它只负责用缓冲区中的key与start数组记录的偏移量生成新的filter data,并插入到result中**,何时使用GenerateFilter由StartBlock对blockoffset的计算结果决定,如果blockoffset被定位到filteroffset.size()上,说明当前block中的数据还需要添加到当前的filter缓冲中,不会GenerateFiler,但如果定位到size()右侧,就使用之前积攒的缓存生成filterdata
- StartBlock必须顺序的接受data block的offset,否则会一次性产生多个空的filter
- Finish负责将filter offset编码到result中,生成最终的filter block结果
- filterblock的编码格式

    ```cpp
    filter 0
    filter 1
    ...
    filter n
    filteroffset 0 (int32)
    filteroffset 1 (int32)
    ...
    filteroofset n (int32)
    arrayoffset (int32)
    ```

**紧密贴合的数据段+由int32组成的偏移量数组+尾部的arrayoffset是leveldb数据段设计的基本模式**

- **这种模式下，对于数据结构的访问只需要该数据段的头指针data以及偏移量数组的offset arrayoffset，就可以定位数据和偏移量数组,具体可以参考Blockiter, 其含有data_指向Block的头部，然后从尾部读取出restart_offset,后续对于原始数据和数组的访问都是靠data+偏移量得到的。**
- 这种模式在编码的时候使用2个成员变量缓存结果
    - block_builder使用buffer保存数据段，用vector<uint32_t> restart保存offset数组，然后将restart编码到buffer中
    - filterblock_builder使用string保存插入的key,使用vector<int>保存偏移量数组，也是一个思路

- **当除数和余数是2的幂时,可以使用移位代替除,用&代替取余数** (所以尽量使用2的幂,降低计算代价)
    - t=2^11 a/t=a>>11
    - t=2^11 a%t=a&(t-1)
- leveldb中的每一个接口的实现都配有一个返回值为基类指针的对象生成函数,然后将接口的实现和对象生成函数的实现放到cc文件中,对外只暴露一个iterator接口和对象生成函数, **大大简化了每个实现的头文件**,比如对于迭代器接口:
    - two_level_iterator.cc中包含了TwoLevelIterator类和对象生成函数NewTwoLevelIterator的实现包
    - two_level_iterator.h中只包含NewTwoLevelIterator的声明
    - block.cc中包含了blockIter类和Iterator* NewIterator(const Comparator* comparator);的实现
    - block.h在block类的声明中包含NewIterator的声明
- two_level_iterator:
    - 该迭代器负责在blocks中遍历entry,需要2个迭代器,index_iter负责根据遍历block_handle,block_iter负责遍历block中的entry,还有一个生成block_iter的block_function,该函数接受index_iter的value()产生的block_handle和其他参数( **注意!遍历index block其value就是block handle**),生成遍历block_handle指向block的block_iter,2个迭代器都可以用key 定位(seek)
    - **index_iter(其本质也是block_iter)负责index_block,block_iter负责普通的data_block**
- merge_iterator:
    - 该迭代器是多个子迭代器的封装,可以按照顺序遍历所有子迭代器中的值(前提是默认每个迭代器的遍历的key都是有序的),由于需要在多个迭代器中进行正反向遍历, **方向便成为该类的重要概念**Next,Prev的前提都是方向正确,所以交叉调用Next,Prev非常耗时,每一次普通的Next也需要进行一次child的遍历操作,效率不高
    - 该类重要函数:
        - FindSmallest(); 将current指针定位到当前key最小的iter上
        - FindLargest();
        - Seek 在定位时,先让每个child seek,然后使用Find函数定位,再调整方向,注意Seek调整的位置是key≥tar
        - Next(); 如果方向为Forward,就让current next一下,然后FindSmall即可,如果方向错误,需要将方向调整回来,这需要将其他非current的child iter全部定位到current->key上并调整到current之后,这样保证开始时current→key应该是最小值
        - Prev(); 如果方向为BackWord,就让current prev一下,然后FindLargest即可,如果方向错误...
- leveldb的类代码中存在着很多生命周期相关的注释,标明了那些类的生命周期应该长于哪些类,看来这种人工进行生命周期管理的方式也是被允许的

- **[table.cc](http://table.cc/)** table解码器
  - Rep,table专用的内部数据结构,table的所有成员变量都保存在table::rep中
      - 由于table.h是用户接口, **为了保证二进制兼容性**我们pimpl手法将成员变量都封装在Rep中,只暴露几个非virtual的接口函数,Rep在table.cc中定义,随着库文件的更新,用户的二进制代码无需变化,实现二进制兼容
      - table.h依赖的头文件也变少了
      - 参见linux多线程服务器编程P444
  - Open
      - 使用文件对象读取table文件,注意这里需要文件大小
      - 通过offset读取footer,得到index block handle,和meta index block handle
      - 使用ReadBlock读取indexblock
      - 初始化table的cache_id然后调用ReadMeta
      - 根据option确定该table是否要使用缓存,如果不使用block_cache为nullptr,如果使用就分配一个Newcacheid
      - 调用ReadMeta读取mete index block
  - ReadMeta
      - 根据meta index blockhandle读取Meta index block
      - 使用block iter找到meta index entry中的filer项,获取filter block handle
      - 调用ReadFilter读取filter block
  - ReadFilter
      - 读取filter block,为rep->filter_data赋值
      - 解码block,获取FilterBlockReader对象,并赋值给rep->filter
  - BlockReader
      - 负责将index block中的value(data block handle)转化为data block iter,涉及data block的读取操作
      - 如果table允许使用缓存就 **先从cache中用key查找block**,key为cache_id和该block的offset的组合,若缓存未命中或不使用缓存就直接调用ReadBlock函数,这里就是cache与data block读取的结合
      - **BlockReader作为table 所创建的iter的block_function参数,table使用two_level_iter,index_iter是table的index_block的iter**
      - table.cc实现了用于two_level_iter的BlockReader,该函数实现了基于LRUCache的datablock读取操作，并返回iterator,**为了保证读取block时分配的内存可以安全析构，block的生命周期交给cache和其引用计数管理，并且作为使用该block的iterator,需要注册RegisterCleanup**，**以在析构时及时unref**,释放Block占用的内存空间，这也是**leveldb基于cache的读取block资源的内存管理策略**
      - 同理，table在读取后也会保存到tablecache缓冲中，leveldb也为tablecache实现了一个Cache类，负责实现基于tablecache的tableReader和iteator
      - 可以看到Block.h以及table.h和其自己的缓冲是解耦的，因为Block.h的实现不依赖BlockCache,table.h的实现也不需要用到TableCache
      - 在table中实现blockreader也是因为tableiter的实现离不开blockreader,blockreader的cache注册也需要table的cacheid
  - InternalGet
      - 在table中根据某一个key查找entry并处理,当使用获取到的data block_offset结合key使用filter快速判断该key在该block中是否存在,从而免去了使用迭代器的搜索过程
      - 一个问题,如果真的找不到key,传入的handle_result的可能是其他key和value...?
      - 这个函数干什么用的? 用于get请求
- table_build.cc table编码器
    - Rep,table_builder专用的内部数据结构,table_build.cc的所有成员变量都保存在tablebuild::rep中
        - 由于table_build.h是用户接口,为了保证二进制兼容性我们pimpl手法将成员变量都封装在Rep中,只暴露几个非virtual的接口函数,Rep在table_build.cc中定义,随着库文件的更新,用户的二进制代码无需变化,实现二进制兼容
        - table.h依赖的头文件也变少了
        - 参见linux多线程服务器编程P444
    - 使用pending_index_entry,当调用Flush后该变量为true,在addkey时使用last_key和key为新flush的block插入index entry,可以让entry中保存的key更短,减少索引block的空间占用
    - WriteBlock专门用于写datablock,主要负责block的压缩,真正的block写操作在WriteRawBlock,WriteRawBlock返回写入block的blockhandle,并为block添加尾部的type和crc校验
    - **当看见下一个block的第一个key时我们才为index block生成当前block entry的key，这可以让我们在indexblock中使用更小的key,这也是为什么我们使用block尾部的key作为index block的key,如果使用头部key就显得不太自然。**
    - tablebuilder的writeblock函数在写入后会传出写入位置的blockhandle,比较方便,比如写入的data block的block handle保存在pending_handle中，后续会添加到index block中,index_block的写入handle地址会再次编码到footer中,计算每次写入的handle只需要位置当前文件写入的offset即可(r→offset)
- leveldb文件介绍
- .log文件 log文件保存最近的数据更新,当log文件到达预先决定好的大小后,memtable写入新的sorted table,旧的memtable和log文件删除,一个新的log文件来接受以后的更新.
    - log文件在这里作为预写日志,防止系统崩溃导致数据丢失,一旦sstable被写入,旧的log就可以被丢弃,新的log接受新的memtable的修改操作
- .db文件 .db文件就是sorted table
    - 多个sorted table被组织多个level
    - level0(young level)中的sort table 允许重复key的存在,当level0的长度到达4时, **所有**处于level0层的文件被合并到level1中
    - level>=1层的socktable不包括重复的key,对于L>=1的每一层,当该层的大小超过10^LMB时,在level-l中的一个文件与level-(l+1)中的所有包含重复key范围的文件合并,这些合并操作逐渐将新的update从上层向下层传递
- MANIFST文件
    - 保存了组成每一层的sorted table文件集合和对应的key range以及其他重要的元数据
    - 每当数据库被打开时,一个新的MANIFEST文件被创建,MANIFEST文件的格式为log,对服务状态的改变会被添加到该log中
- current是一个简单的文本文件保存最近的MANIFEST文件
- LOG文件
    - info message被输入到LOG和LOG.old中
- Level0写入管理
    - 内存中有memtable,与log文件对应,当memtable更新,log文件记录更新操作
    - 当log文件更新到4MB时,创建新的memtable和log文件
    - 将旧的memtable写成新的sstable
    - 抛弃旧的log文件和memtable
    - 将新的sstable放到level0
- 压实操作细节:
    - 当压实的输出文件的长度>2MB或key覆盖的范围>10个level-(L+2)文件,就生成新的level-l+1文件,直到输出完毕. 对于key覆盖范围的限制主要为了防止level-l+1的压实操作不会覆盖太多的level-l+2文件,降低压实速度
    - 对于压实操作使用的旧文件会被抛弃,并且新产生文件会被添加到服务状态(MANIFST)中.
    - 压实操作会抛弃重复数据,如果没有更高编号的级别包含范围与当前键重叠的文件，它们还会删除delete marker(防止delete marker堆积在底层)
- 压实操作的代价估计:
    - 对于level0,最坏情况的压实:将level0的4MB数据与level-1的10MB(整层)数据进行压实,且key无重复,也就是读14mb,写14mb
    - 对于level l,最坏情况的压实:将levell的一个2MB文件与level-l+1的12个文件进行压实,level-l+1中为12是因为一个level-l的文件的key范围不能超过10倍大小的level-l+1文件,但是level-l的文件key range不可能完全与下一层的10个文件范围正好对应,可能会有2个边缘文件,所以最多会选中12个文件,也就是说压实操作会读26mb,写26mb
    - 假设一个磁盘io的速度是100mb/s,那么最坏情况下压实操作需要0.5s,如果我们限制background线程的io速度,比如10%的full 100Mb/s,那么一个压实操作的最坏时间为5s,如果用户写入速度为10mb/s,那么每5s 压实26mb,却要写入50mb, **这会导致在level0层积累大量含有重复key的文件,大大降低读取的速度** 高速写入时的读取速度下降是最本质的问题.
- 最坏情况的解决策略:
    - 使用更大的log切换阈值,就是说让更多的数据可以保存在memtable中,缓解大量数据写入操作造成的数据读取困难
    - 当level0文件多到一定程度,降低写入速度
    - 尝试降低大型合并操作的代价,也许大多数的level-0文件将他们的block保存在缓存中,可以利用缓存来降低合并操作的复杂度
- 恢复操作
    - 读取CURRENT查找最近提交的MANIFEST
    - 读取指定的MANIFEST文件
    - 清理过时文件
    - 我们可以在这里打开所有sstables，但是最好还是偷懒..
    - 将log块转化为新的level0 sstable
    - 将恢复操作写入新的log文件
- 文件的垃圾收集
    - 在每次压缩结束时和恢复结束时都将调用RemoveObsoleteFiles（）。
    - 它查找数据库中所有文件的名称。它将删除不是当前log的所有log文件。
    - 它会删除所有没被某个level引用且不是压缩output的sstable文件 (sstable文件清理)
        - 比如压实操作完毕之后,levell和levell+1 discard被压实的旧文件,注册新文件,旧文件就会被垃圾收集删除
- filename模块,生成和解析leveldb中各种类型的文件名,根据dbname,id,和后缀构造各种类型的文件名,descript文件就是MANIFEST文件:

```
   dbname/CURRENT
   dbname/LOCK
   dbname/LOG
   dbname/LOG.old
   dbname/MANIFEST-[0-9]+
   dbname/[0-9]+.(log|sst|ldb)

```

- table_cache 以文件id为key,TableAndFile为value,实现对table和相应文件描述符的缓存
    - 该类的接口函数需要文件大小是因为Table的Open函数需要file_size
    - FindTable 使用id从cache中获取相应table的handle,如果找不到,就使用文件id拼接文件名直接读取table文件,并插入缓存,获取handle
    - NewIterator 根据文件id获取对应table的iter
        - 由于传出的iterator使用了cache缓存中的数据,为了保证不再使用缓存数据时降低引用计数,需要在该iterator析构时release handle,于是我们在iterator中注册UnrefEntry函数,其实这里可以用bind绑定回调,这里iterator使用了比较原始的方式来记录清理函数
    - Get 对文件id代表的table内的某一个key执行handle_result操作
    - Evict 从tablecache中去除文件id指定的entry
    - table_cache提供了对table类和打开文件的内存资源管理
    - Get函数其实可以用iter实现，但是对于单点查询，我们可以利用table中的filter实现高效的Get函数.
- log文件格式
    - log文件包含一系列32kb大小的block(注意是固定大小!!!),在文件的尾部可能例外的包含一个小的block
    - 每个block包含一系列的记录

    > block:=record* trailer?
    record :=
    checksum: uint32  type和data的校验码,小端,4bytes
    length: uint16 小端,2bytes
    type: uint8 1bytes
    data: uint8[length]

    - 从上述格式看出一个记录最少应该占7bytes(data为空,只有头部),所以一个record不会从block的最后6个byte开始,因为连头部都不够,任何剩余下的bytes会由0字节组成,reader必须跳过这些字节
    - 如果恰好有7个bytes残留在当前block的尾部,而一个新的非空record被添加,则该block只能装得下record的头部,writer必须使用FIRST record(其user data 为0bytes)来填充这7个字节,然后将所有的data放到后续的block中
    - 目前的type类型
        - FULL==1
        - FIRST==2
        - MIDDLE==3
        - LAST==4
    - 一个FULL record包含一个完整user records
    - FIRST,MIDDLE,LAST 使用在user records被分割成多个片段(因为block边界的原因)的情况,FIRST表示user record的第一个片段,LAST表示user record的最后一个片段,MIDDLE表示所有内部的片段
        - 比如对于以下记录:

            > A: length 1000
            B: length 97270
            C: length 8000

        - A被存储为FULL record,处于第一个block中
        - B太大,在第一个block中存放第一个片段(FIRST),第二个片段占据了第二个block的所有空间(MIDDLE),在第3个block中保存最后一个片段(LAST),此时第3个block只剩下6个bytes,全部填充为0
        - C保存在第4个block中,为FULL record
    - log文件采用recordio格式,RecordIO是一组二进制数据交换格式的名称。基本思想是将数据分成单独的chunks，称为“record”，然后在每条记录的前面添加字节长度，然后是数据
    - log文件格式与sstable存在的显著差别
        - **由于sstable一旦生成就不再修改,所以文件格式设计的重点在于提高搜索性能(随机读),无法进行直接的追加操作(尾部都是元数据~,要追加得生成新的sstable)**
        - **由于log文件需要和memtable保持同步,memtable一旦更新,log文件就得追加更新记录(顺序写),所以log文件需要在追加写上高效,所以使用了这种便于追加的格式(recordio)**
- log_reader 用于从log中顺序读取record,并处理各种格式解析错误的情况
    - 注意该类使用的是SequentialFile , 也就说明该类无法重新定位到起点!!!!从头开始顺序读取完毕就没了
    - 因为record可能有多个fragment,log_reader为了简化逻辑,将读取细化为ReadPhysicalRecord和ReadRecord
        - ReadPhysicalRecord负责真正的解码工作,读取block存入buffer,并从buffer中解码出单个record(fragment),并返回record的type类型,该函数处理了许多解码过程中可能出现的错误情况kBadRecord,以及文件结束情况kEof,并将错误情况包含在返回type类型中,
            - 该函数结束条件的基本逻辑是: 如果读取到的block大小小于kBLOCKsize,说明已经进入最后一个block,标记为eof_=true,读完最后一个block就会返回文件结束
            - 返回kBadRecord的情况(ReadRecord遇到kBadRecord会再次循环读取下一个fragment!!):
                - 忽略掉offset在init_offset之前的block,并返回kBadRecord,以便下一次继续解析
                - 校验失败,直接清理整个buffer返回kBadRecord,下次ReadRecord调用时会直接读取下一个block,相当于drop当前block
                - record长度>buffersize,清空buffer,返回kBadRecord,下一次调用直接读下一个block,但是如果已经是最后一个block了就返回kEof,因为已经没有下一个block可供下一次调用了
                - ...
        - ReadRecord根据ReadPhysicalRecord读取的fragement和具体的类型,生成真正逻辑上的record并返回给用户,并对ReadPhysicalRecord标识的各种错误进行处理,注意除了kFirst kMiddle以外如果遇到kBadRecord错误,也会继续读取下一个fragment,不是直接返回!!!
            - 在不出错和不到末尾的情况下,ReadRecord每次循环读取一个fragment
        - SkipToInitialBlock会在第一次ReadRecord时定位到init_offset指定的block上
    - **Slice有点类似缓冲区指针的概念，其本身仅仅用于以移动方式解码或传递指定字符串所在的位置，真正持有字符串数据的都是Block.h或者是调用reader函数时传入的string，由于在解码和数据表示上非常方便快捷所以基本上都使用slice为接口访问(解码）和表示原始数据**
        - 比如在logreader上，我们使用**Slice buffer来标识当前block数据（back_store）中剩余还未解码的部分**。作为区域标识使用，每次解码都会将slice向前移动，而且几乎所有的解码操作都使用了slice移动的操作(有点类似指针移动)，此时back_store保存真正的数据
        - logReader一个block一个block的读取数据，然后使用slice来标记当前的剩余解码位置，slice buffer不够解码或者为空就读取下一个block
        - 在其他场景上比如Block_iter上，data数据成员指向的真正数据由cache和引用计数管理，而Slice value_指向data数组中的一小块标识value的区域，作为变量使用。
- log_write 一次向log文件写入一条record
    - AddRecord中的每次循环都写入一个fragment
    - 真正的编码工作在EmitPhysicalRecord函数中
    - 预先计算了各个type的crc,减少crc的计算量
- 问题: 专用的内存分配器,加快内存分配的速度,减少系统调用,能保证局部性吗,虚拟地址的连贯可以保证物理地址上的连贯吗,一个程序的地址空间和物理空间的关系是什么?栈中的地址空间上分配的数组,其对应的物理空间是否连续? malloc的原理?
- skiplist:
    - **leveldb的所有写操作都要经过skiplist**,skiplist里面记录的是用户传进来的key/value，这些字符串有长有短，放到内存中的时候，大量的分配很容易导致内存碎片。所以使用arena进行内存分配,skiplist控制着写入对象的生命周期，immutable的sstable写入操作中的key，value都来自skiplist分配的内存
    - 这里可以总结出内存分配的来源：读操作分配的内存来自block读取和table读取操作，使用cache和引用计数来对这些分配的内存进行生命周期的管理，写操作使用的内存在skiplist中分配，而skiplist使用arena分配器进行更加高效的内存分配,arena专门为只可追加的skiplist设计在内存分配中呈现出只可追加，无法部分释放的特征。当然这种内存分配的策略不能用于cache,因为cache要根据引用计数来释放不需要的数据，每一个分配的对象都应该可以自由释放。
    - arena并不是为整个LevelDB项目考虑的。主要是为skiplist也就是memtable服务。**碎片化的内存会造成什么问题？？？？**
    - skiplist,memtable是没有删除接口的。是只可追加的结构，所以里面的元素总是不断地添加进来。
    - skiplist,memtable会在生成L0的文件之后，统一销毁掉。所以内存块可以直接由Arena来统一销毁。
    - skiplist的head节点是默认分配最高节点kMaxHeight,当前max_height_表示数据节点中的最高节点,所有的搜索操作都从head的max_height_-1的位置(数据段最高位)开始搜索
    - skiplist FindGreaterOrEqual提供prev是为了方便insert时将new node插入到list中
    - skiplist的多线程安全:
        - skiplist的node节点中的next数组指针是atomic的,使用原子性来保证线程安全，且Set_Next,Next操作使用了内存同步来保证不会被乱序执行,在skiplist上使用的原因是skiplist是一个可读可写的结构，多线程在需要共享读写该skiplist进行同步操作时，不适用内存同步会引起同步失败，产生逻辑上的问题。
        - 在有些无需使用skiplist进行同步的场合，我们可以使用NoBarrier_Next和NoBarrier_SetNext
        - insert函数被设计成无需额外的读写同步,具体如下:
            - 对于 max_height_的原子修改,无需和读操作同步:
                - 如果在此时存在一个并行的reader,并读取到了max_height的new value ,那么有2种情况,要么header为nullptr也就是header还没更新,在reader时会直接下降到下一层,如果header更新了说明node已经链接完毕(new node的后方node最后链接)
            - **对于新node加入链表的流程**:
                - 原子操作、从底层连接、先连前面的node再链接后方node使得reader在读取时不会遇到与指针相关的竞争问题(lock free)
                - 和普通链表可以实现的lock free一样，只要new Node节点底层的第一个节点被连接到skipList上，即使其他节点还没有连接完成，reader依然可以遍历到该node，也不会影响安全性，所以不需要额外的mutex加锁insert的连接过程就可以实现读写的同时进行。
                - SetNext和Next也进行了同步操作
        - insert函数的巧妙设计保证了skiplist可以无需额外的的insert,读同步,也就是说该数据结构是读写free lock,读读free lock,写写需要同步的结构(insert操作需要读和写，这2次读写操作必须一起进行）,这样的设计是为了提高skiplist的效率,毕竟所有的读写操作都是从skiplist开始
    - 注意skiplist只保存一个value,所以memtable使用该结构时需要将entry编码到一个slice中,作为key传给skiplist保存

- db_format
    - ParsedKey保存的是已经解码完成的Internalkey，基于ParsedKey构造了InternalKey的编码解码函数 （其设计思路类似于Blockhandle这种小的format,将编码解码器都集中在一起）
    - InternalKey
        - **internalkey是值类型的。**
        - InternalKey保存了InternalKey的编码字符串，为该字符串提供了访问接口
        - leveldb中存储的key全部为InternalKey
        - internalKey由userkey,seq,type组成，其中在编码上，seq和type共同组成一个固定的64位数据段(8字节，这样的好处是不需要为user_key提供长度字段也可以获取user_key本身，参见ExtractUserKey)
    - InternalKeyComparator
        - 该比较器提供了对internalKey的比较规则
        - 对于userKey的比较，比较器包含了一个user_comparator_,user_comparator定义了user_key的比较规则
        - 对于seq,type字段默认进行降序比较，这样做的好处是在顺序遍历key集合时，对于user_key相同的internalkey,总是遍历到序列号最新的internalkey，我们基于InternalKeyComparator所定义的升序来对InternalKey进行升序排序，所以在Compare的实现中，我们要让序列号的对比结果按照相反数返回，这样userkey比较相同的情况下先遇到的就是最大序列号
        - FindShortestSeparator会将internal的userkey提取出来，调用user_comparator的findshortest,然后重新拼接出一个internallkey
    - lookupkey,如果需要将internalkey作为一个结构单独编码进文件，由于internalkey是变长的，我们需要在前面添加length,也就是lengthprefix,lookupkey提供lengthprefixinternalkey和internalkey的同时访问,lengthprefixinternalkey需要用在memtable中，而一般的internalkey则用在block中的data entry中，data entry中的key已经有entry头部的固定字节指示大小，不需要自己额外添加长度标识
        - 在解码时，我们使用GetLengthPrefixSlice将Internalkey的字符串提取出来
        - 所有LengthPrefix的结构都可以使用GetLengthPrefixSlice进行解码
    - lookupkey是dbimpl中get函数将user_key编码为internalkey的基本方法,使用比较方便
    - 使用,,R来全局搜索Slice类的主要用法
        - **ExtractUserKey使用了const Slice&作为参数，该参数既可以接受Slice又可以接受string，体现了Slice另外一个好用的地方。**在Add（memtable add）和写(write)操作上,参数为Slice&,这样传入String或Slice都可以，方便高效
        - 在coding.cc中的解码函数和env_posix.cc的读取相关函数中，Slice*传入用来接受移动过的待解码区和指向缓冲区scratch的Slice

- memtable
    - memtable使用InternalKeyComparator,保存的key也是internalkey,使用LengthPrefix的编码方式
    - 使用引用计数,析构函数私有
    - add根据userkey,value,seq,valuetype将用户key编码为internalkey
    - Slice这个对象如果要编码到文件中且可以被识别，需要加上长度前缀，也就是lengthPrefixSlice
    - memtable将internalkey和value编码为可以保存到skiplist中的key,skiplist中的key格式为:

    ```
      key_size     : varint32 of internal_key.size()
      key bytes    : char[internal_key.size()] // key_byte中保存的internal_key
      value_size   : varint32 of value.size()
      value bytes  : char[value.size()]
      // internal_key的结构是:
      userkey  
      tag      :uint64 (seq+type)

    ```

    - memtable也为skiplist注册了专用的key比较器MemTable::KeyComparator先从key中解码出internal_key,再使用internal_key比较器比较二者的值,返回该比较结果
    - DB数据在内存中的存储方式，写操作会先写入memtable，memtable有最大限制(write_buffer_size)。LevelDB/RocksDB的memtable的默认实现是skiplist。当memtable的size达到阈值，会变成只读的memtable(immutable memtable)。后台compaction线程负责把immutable memtable dump成sstable文件。
    - Slice有点类似缓冲区指针的概念，其本身仅仅用于以移动方式解码或传递指定字符串所在的位置，真正持有字符串数据的都是Block.h或者是调用reader函数时传入的string，由于解码非常方便所以基本上都使用slice为接口访问数据

    - 线程安全等级
        - 不可变
            - 1.不可变的对象一定是线程安全的，无论是对象的方法实现还是调用者，都不需要再采取任何的线程安全保障措施。
            - 2.不可变带来的安全性是最纯粹的最简单的。
        - 绝对的线程安全
            - 不管运行时环境如何，调用者都不需要任何额外的同步措施。
            - 通常代价是很大的，容易不切实际。
        - 通常来说线程是安全的，但对于一些特定顺序的连续调用，就可能需要在调用端使用额外的同步手段来保证调用的正确性。(如读读安全,但是带上写就需要对所有访问接口进行同步)
        - 本身不是线程安全的，但是可以通过调用端使用同步来保证安全性。
        - 无论是否采用同步措施，都无法在并发中使用。
    - version_set 构建了整个level体系,对table,level,key range,compaction,还包括使用manifest持久化version状态等操作,下面是一些主要的概念介绍
    - SequenceNumber: LevelDB中每次写操作(put/delete)都有一个版本，由sequence number来标识，整个DB有一个全局值保存当前使用的SequenceNumber，key的排序以及snapshot都要依赖它。
    - Version: **一套以版本变迁为核心的数据管理方式**,将每次compact后的最新数据状态定义为一个version，也就是当前DB的元信息以及每层level的sstable的集合。跟version有关的一个数据结构是VersionEdit，记录了compaction之后的状态变化，包括删除了哪些sstable，新增了哪些sstable。old version + versionedit= new version。整个DB存在的所有version被VersionSet数据结构保存，这个数据结构包含：全局sequencenumber、filenumber、tablecache、每个level中下一次compact要选取的start_key
    - Version由引用管理,compaction操作进行时会ref其操作的当前version
    - 压实前->压实后的状态变更:增加了压实输出levell+1的文件,并将压实的输入文件全部删除
    - 状态变更的过程:compaction输出versionedit,versionedit交给version,来完成old version+versionedit=new version的操作
    - FileNumber: DB创建文件时将FileNumber加上特定的后缀作为文件名，FileNumber在内部是一个uint64_t类型，并且全局递增。不同类型的文件的拓展名不同，例如sstable文件是.sst，wal日志文件是.log。 **使用filename来代替文件名表示各种文件,比较统一和方便**(为何不用文件描述符? :文件描述符主要是描述打开文件用的,但是我需要的是一个 **文件存储中唯一的文件标识**,根据文件描述符的机制,使用文件描述符非常容易创建出同名文件)
    - Compact: DB有一个后台线程负责将memtable持久化成sstable，以及均衡整个DB各个level层的sstable。compact分为minor compaction和major compaction。memtable持久化成sstable称为minor compaction，level(n)和level(n+1)之间某些sstable的merge称为major compaction。
    - FileMetaData 管理version变革时sstable的**文件表示**,包含了**文件号**,文件key范围集合, **文件的引用情况**,在compaction时需要将该类数据作为compaction的输入,其生命周期由自身的ref管理,version的ref到0时触发析构函数,析构函数会降低其持有的FileMetaData的引用计数,如果下降到0,就析构该FileMetadata对象.多个version可能共享同一个FileMetadata(参见version_set中的version表)
    - version_edit **用于记录一次compaction对原本descriptor进行的状态变更**,包括新增的文件和删除的文件,对文件描述上主要包含table文件的level,文件号,文件key范围集合(FileMetaData )等变量
        - 相对于压实以前的状态,压实以后的状态变更很简单,也就增加了压实输出levell+1的文件,并将压实的输入文件全部删除即可
        - version_edit整个编码就是对其成员变量进行最简单的顺序 **变量编码操作**,格式大致为:

        ```
          type: v32
          length: v32
          data : 变量数据
        ```

        - 这种编码我记得在网络编程中也遇见过...
        - versionEdit提供了自己的编码解码操作,根据前缀判断类型和长度
    - Version
        - block的生命周期使用由blockcahche和blockcache记录的引用计数管理，table也同理，version在生命周期上自带引用计数，到0就释放version,和LRUhandle一样，Version被链接在一个链表中（next_, prev_）,并持有其所归属的VersionSet指针,version在析构时会从链表中删除然后减少自己持有的FileMetaData的引用计数，如果引用计数减到0也会释放FileMetadata
        - GetOverlappingInputs将level层被begin~end所覆盖的文件全部保存到inputs中,是OverlapInLevel的细化
            - GetOverlappingInputs对level0层有特殊处理
        - PickLevelForMemTableOutput: 为Memtable的输出挑选sstable的放置层数,我们要避免level0到level1中trival的合并,可以在memtable->sstable的时候将sstable插入到指定的层来避免该多余的合并
            - 根据kMaxMemCompactLevel发现memcompactlevel最大可以插入的层数是第2层
            - 将memtable的sstable文件保存到一个层有2个硬限制: 1.文件范围在该level不重复 2.该文件的文件范围不能覆盖超过下一层文件总大小指定限制
        - LevelFileNumIterator对于给定的version和level,遍历该level上的所有文件
            - key()返回的是当前文件的largest key
            - value()返回的是 16bytes的包含filenumber(8byte)和filesize(8bytes)数据
            - 为了避免在生成value的过程中分配编码空间,直接预先分配可变缓冲区
                - mutable char value_buf_[16];
        - **NewConcatenatingIterator,将LevelFileNumIterator与table iter结合**,使用GetFileIterator作为转化函数,在获取table的过程中还使用tablecache缓存, **实现了level级别的key value遍历**
            - Block(blockiter)→table(tableiter)→version(leveliter)
            - version暂时和memtable没有关系，它只保存了一个版本下各level文件的集合，相当于table的上级，也是不可修改的，由于版本变迁的存在，versionEdit+version=newversion的这种版本变迁相当于copy on write
            - 和table iter将index iter与block iter结合是一样的道理,使用blockreader转化函数,在得到block的过程中使用block cache获取已经缓存的block,若未缓存block,就直接读取
            - tablecache和blockcache相比,tablecache将未命中读取文件的情况实现在内,并直接从tablecache中就实现了new tableiter的功能,而blockcache仅提供缓存查找功能,真正的查找缓存、读取block,产生iter的功能全部由blockReader实现, **可以看到处于偏底层的blockcache的粒度更细些,因为block_cache是一个可选功能**
        - **Get(和table的InternalGet以及tableCache的Get一样的形式)**用于从version中根据key查找value,之所以使用get方法并在内部使用tablecache的get方法在version中查找而不使用迭代器,是因为迭代器的查找无法记录查找的状态,使用get函数结合回调可以记录最终查找的结果
            - **注意Get底层的Table::InternalGet是blockIter的seek函数实现的,所以底层实现其实也是用迭代器**,但是**InternalGet使用了基于filter的筛选操作，**Table::InternalGet会回调Saver函数,记录状态
            - Get函数的精髓在于回调函数的大量注册: 为了获取底层Get所返回的value,一般的函数可能需要一层层的返回，Get函数采取注册回调函数并传入void参数，在回调函数中记录查找状态。
            - Get函数在内部定义了State对象,State对象包含了对table文件进行操作的动作回调函数Match
            - Match函数调用tablecache的Get函数注册Saver函数回调,将查找结果与相关状态保存在State的saver对象中
            - 然后调用ForeachOverlaping函数,并将state对象与Match函数注册到该函数中,Match的arg代表state对象指针
            - 注意Match在Found时返回false,主要为了保证ForeachOverLaping可以在Match Found时返回结果而无需继续遍历
            - ForEachOverlapping函数的典型方法,**定义State类记录每次调用的结果,定义包含State指针的Match函数,Match函数每次调用的结果都会被保存到定义的State类中**
            - 在match函数的不断匹配回调中，如果seek_file==nullptr且last_file_read≠nullptr,说明上一个match的文件并没有找到指定的value，上一个文件发生了seekmiss的情况，于是我们在GetStats对象中记录发生seekmiss的文件以及所在的层数，**以便DBImpl::Get在调用get函授后调用UpdateStats更新当前version的seek_miss**
            - version中的file[0]中的文件是有序的吗? 版本管理的整个流程梳理一下 ([https://izualzhy.cn/leveldb-version](https://izualzhy.cn/leveldb-version))
            - 如果一个函数需要层层递归才能得到最终的结果，我们可以使用State类附带回调函数的形式，将回调函数和State作为该函数的参数,（其实可以不讲state作为参数，直接使用std::bind,不过这样在兼容性上会有些问题）
        - Version::UpdateStats 对发生seek miss的文件进行处理,降低该文件MetaData的allowed_seek值，当miss累计到使得allowed_seek为0时，我们将该文件标记为当前Version需要被seek compact的文件（file_to_compact_）
        - Version::RecordReadSample使用ForEachOverlapping记录第一次match的State,找第二个match,如果match一次，说明一定会命中，不用操作，如果命中match 2次，说明以后的搜索中，在第一个文件中可能出现未命中然后要到第二个文件中去搜的情况，需要我们调用updatestats去更新allowed_seek的状态。
        - GetFileIterator和BlockReader差不多，基于cache返回迭代器，一个在version中实现，一个在table中实现,都作为twolevelterator的handle函数
    - VersionSet
        - 压实操作的流程:
            - 首先使用Finalize计算哪个level要进行压实操作
            - 使用PickCompaction生成Compaction对象，从第一步的level中根据compaction_pointer确定Compaction操作第一层的文件,如果是level0，就需要把与指定文件重合的所有文件都放到Compaction的第一层
            - 使用SetupOtherInputs计算Compaction的第二层需要压实的文件（中间大量使用GetRange和GetOverlappingInput函数），根据情况还可以扩展第一层
            - Compaction迭代器可以实现对Compaction内部元素的顺序遍历，如果第一层为level0元素，就将所有level0层的文件构造迭代器并使用聚合迭代器与第二层的level迭代器聚集在一起，如果不是level0就直接将2者的level迭代器聚集在一起即可

        - VersionSet::Builder 负责实现版本的变迁，主要为了实现version+Edit集合(builder收集)⇒builder⇒new version,其中Apply用于聚合Edit,SaveTo用于生成新的version,SaveTo的原理如下:
            - SaveTo本质上是oldVersion+build(包含多个edit的更新)=newVersion
            - SaveTo遍历oldVersion与build的每个level,对于每个level,按照smallest的顺序(使用带比较器的set实现)对oldversion中的旧文件与build中的add_file新文件调用MaybeAddFile
            - MaybeAddFile判断当前文件是否可以添加到newVersion中,判断的依据是只要build的删除文件集合没有找到该文件，说明该文件就可以添加
            - 在正常的版本变迁中，level>0的new file是不可能发生重叠的，SaveTo和Maybeaddfile也提供了检查重叠的手段，一旦重叠就报错
            - leveldb的版本变迁由于sstable的不可变性变得非常简单，只要记录一次compaction操作后删除的文件和添加的文件即可。
        - VersionSet::AppendVersion将v设置为当前version,并添加到versionlist 中
        - VersionSet::LogAndApply使用单个edit更新版本状态
            - 接受一个edit，根据current version生成new version,并将该edit写入description文件做记录，再将该new version 作为current version添加到verset中，同时使用edit中的log_number等信息更新verset状态
            - **description文件（Manifest文件)是log文件**，该文件记录的内容是versionedit的编码字符串，用于保存versionset的版本变迁情况
        - VersionSet::Recover读取description文件更新版本状态
            - 读取current description文件，获取保存的edit记录,全部apply到builder中,并SaveTo newversion,为该version计算compaction level然后调用AppendVersion将version设置为currentversion,并添加到链表中
            - 一个version可以从manifest file中解码出来
        - VersionSet和VersionEdit中的log_number,prev_log_number，next_file_number,last_sequence是不是用来记录版本状态的，VersionEdit中记录了每个版本变迁时log,prev_log,next....等信息的变化，而VersionSet保留了当前版本这些文件信息的状态
        - VersionSet::Finalize 预先计算new version的best level,为下一次的size compaction做准备
            - best level是根据score最大的level确定的，是当前最需要进行size compaction的level层
            - 我们通过限制文件数而不是字节数来特别对待level0有一下原因:
                - 对于较大的写缓冲区，不用进行太多的0级压缩
                - level0中的文件在每次读取时都会合并，因此我们希望在单个文件大小较小时避免使用太多文件（可能是因为写缓冲区设置小，压缩率非常高或压缩率很大）覆盖/删除）。
        - VersionSet::WriteSnapshot 将current version的相关状态(compact_pointer_,files)全部写入edit,然后将该edit写入log
        - VersionSet::AddLiveFiles 遍历version list中每个version中的文件,将文件号添加到live中
        - VersionSet::MakeInputIterator 构造遍历Compaction的2个input level的key-value的iter,先构造iter数组，如果是第0层就一个一个将table iter添加到数组中，否则就直接用level迭代器，最一起后使用mergeiter将数组中的迭代器组合在一起
        - VersionSet::PickCompaction 为当前verset生成一个Compaction对象，优先度为size compaction>seek compaction
            - 对于size compaction,如果currentversion中的compaction score>1,就生成compaction对象，获取current version的compaction_level来决定压实的文件所在的level，再根据verset中的compact_pointer来决定选中哪个文件
            - 对于seek compaction,如果当前version不需要进行size compaction,查看current version中的file_to_compact是否有效，若有效就将level确定为current version中的file_to_compact_level,压实的文件确定为file_to_compact
            - **对于level==0的特殊情况，会获取level0中所有与当前选中文件重叠的文件也插入到input[0]中，处理level0中特有的重叠情况，不重叠的就不插入。（看来并不是将level0层的所有文件一起压缩，而是选择相互重叠的文件一起压缩)**
            - **注意上述current version中决定compaction种类和具体位置的状态参数(compaction score...)都是在版本变迁时(old version+edit=new current version)时获得更新的**
            - 二者的后续操作相同，调用SetupOtherInputs
        - VersionSet::SetupOtherInputs 填充Compaction的input[1]层
            - 先使用addboundaryinput将存在边缘条件的file添加到input[0]
            - 获取input[0]的整体key范围,使用该整体范围获取level+1层的所有文件，添加到input[1]中
            - 获取input[0]和input[1]的整体范围，再看看在**不改变level+1合并文件数**量且未超过合并文件大小限制的情况下能不能在level中多合并几个文件
            - 根据整体范围再获取下下层的文件，用来在压实的时候控制输出文件与下下层的重叠量
            - 更新verset的compact_pointer为level层被选中key范围的largest internal key,并更新将Compaction中的edit的compactPointer
            - AddBoundaryInputs 边缘情况处理，用于compaction时的边界条件,因为internal不相等不代表user_key不相同,**用于表示sstable的边界的internal key可能存在userkey相同但tag不同的情况**,这样对于2个文件,如果b1=(l1, u1) and b2=(l2, u2) and user_key(u1) = user_key(l2), 但我们在compaction时没有压缩b2文件,会导致压实不完全,所以要把存在这种情况的边缘file也提取出来进行压实
        - VersionSet::CompactRange 手动Compact类型，根据输入的范围指定input[0]要压实的文件，如果input[0]的文件大小较大，需要避免一次压实过多的文件。但是我们不能对0级文件执行此操作，因为0级文件可能会重叠，并且如果两个文件重叠，我们就不能选择一个文件并删除另一个旧文件。
    - Compaction
        - Compaction::IsBaseLevelForKey 如果从本次Compaction对应的grandparent层开始没有文件在user_key的范围内,返回true
    - Compaction 从大的类别中分为两种，分别是：
        1. MinorCompaction，指的是 immutable memtable持久化为 sst 文件。
        2. Major Compaction，指的是 sst 文件之间的 compaction。
    - 而Major Compaction主要有三种分别为：
        - Manual Compaction，是人工触发的Compaction，由外部接口调用产生，例如在ceph调用的Compaction都是Manual Compaction，实际其内部触发调用的接口是：

            ```
            void DBImpl::CompactRange(const Slice begin, const Slice end)
            ```

        - Size Compaction，是根据每个level的总文件大小来触发，注意Size Compation的优先级高于Seek Compaction，具体描述参见Notes 2；
        - Seek Compaction，每个文件的 seek miss 次数都有一个阈值，如果超过了这个阈值，那么认为这个文件需要Compact。
    - Compaction优先级(压实调度,有点进程优先级和进程调度的感觉)
        - 这些 Compaction 的优先级不一样（详细可以参见 BackgroundCompaction 函数），具体优先级的大小为：
        - Minor > Manual > Size > Seek
        - LevelDB 是在 MayBeScheduleCompaction 的 Compation 调度函数中完成各种 Compaction 的调度的，第一个判断的就是 immu_ （也就是 immutable memtable）是不是为 NULL，如果不为 NULL，那么说明有 immutable memtable 存在，那就需要优先将其转化为 level 0 的 sst 文件，否则再看是不是 Manual，否则再是PickCompaction() 函数——它的内部会优先判断是不是有 Size Compaction，如果有就优先处理。具体每一种Compact的细节，下面一一展开。
    - Seek
- write_batch 实际上就是一个操作**编码器**，接受输入，编码rep,预先日志中保存的就是这种操作编码，也就是log文件中的每个record其实就是一个write_batch!!!!! 这是符合WAL的定义的，WAL中保存的是操作本身而非只有数据！！！
    - write_batch维护了一个缓冲区，可以将Put和Delete操作编码到rep中
    - 为了对write_batch进行一些操作，又不想将这些操作函数暴露在公开的write_batch接口上，我们使用额外的writeBatchInternal类实现了一批操作函数。internal类是write_batch的有元，这样对于内部操作的修改不会影响到
    - write_batch的iterate()函数遍历rep所有值,并使用handle执行对应的操作，MemtableInnserter继承了WriteBatch::handler并实现了Put,delete,在WriteBatchInternal::InsertInto中，我们构造了MemtableInserter将该handle作为参数执行Iterate函数可以将Writebatch的操作对指定的memtable执行
    - WriteBatchInternal::Append 直接在编码上合并2个writeBatch,这样就不用使用迭代器一个一个合并，效率更高.
    - WriteBatch::Insertto 用于将WriteBatch中的内容插入到memtable中
    - 我们既要求WriteBatch可以读取又要求可以写入，所以

- builder BuilderTable 使用iter构造从memtable中构造table文件,并根据meta->number命名,若成果,meta的剩余部分会被填充
- db_impl
    - 可以看出versionset管理了各版本sstable,manifest文件，以及memtable的log文件的号码
    - SanitizeOptions 对用户提供的option做一些数据库的默认设置修正
        - 方便直观的大小表示方法64 << 10（64kb）1 << 30(1gb)....
    - Recover操作 首先读取Manifest文件来进行sstable恢复，然后检查从Manifest中获取的table文件记录对应的真实文件是否存在，最后读取≥当前log和prev_log文件号的log文件来进行恢复操作.(DBImpl::RecoverLogFile)Recover
    - RecoverLogFile 将log文件中的每个WriteBatch写到memtable中（使用batch),并根据读取到的每一条记录更新max_seq,如果是最后一个last_log要考虑是否重用该log
    - WriteLevel0Table 使用参数mem中的内容生成sstable
        - 为了降低锁粒度减少临界区范围，我们在buildtable时解锁，执行完毕再加锁，为了防止table文件在解锁时被垃圾回收删除在解锁前使用pending_output记录该文件来防止被删除，在函数执行完毕，重新上锁之后将其从pendingouput中删除，删除之后函数处于加锁段，所以不会被错误的进行垃圾回收
        - 该函数貌似没有进行版本变迁，只是用edit记录状态变化，真正的版本变迁操作在CompactMemtable函数中执行
    - MaybeScheduleCompaction  当存在manual compaction任务和version自发的NeedCompation任务时开启BGWork进行压实操作(imm的压实在哪个背景线程执行？)
    - BGWork启动BackgroundCall，BackgroundCall 调用BackgroundCompaction进行压实，为了防止压实后后续的level存在过多文件，再次调用MaybeScheduleCompaction,调用完成后会发送background_work_finished_signal_信号
    - background_work_finished_signal_信号可以管理多个状态变化，只要监听该信号的函数添加上附加的条件即可（在while条件区域中添加)
    - db_impl中包含状态标识has_imm_,该状态标识是原子性的，而且在无锁情况下存储和加载该原子变量需要内存模型的同步，来防止在乱序执行的情况下，has_imm_为true,但是imm_却还为false或者相关的变量还没有准备好的情况。
    - DoCompactionWork 遍历input file中的每一个key,根据一定的规则保留相应的key,在一定的限制（ShouldStopBefore）下消耗哦构造输出文件。并统计压实过程消耗的时间，具体规则见snapshot
    - CompactionStat 负责保存压实过程中必要的信息，比如输出文件的元信息，输出文件对象，输出文件的tablebuilder,压实过程中需要维护的smallest_snapshot,以及包含input对象的compaction
    - CompactionStats负责保存一次压实操作的时间消耗，db_impl中保存了一个stats数组，用于记录每一层的时间消耗
    - NewInternalIterator 汇集mem,imm,以及各层的leveliter,组成merge iter，对db中的元素进行遍历，为了保证memtable和相关的version不会在迭代时被销毁，会在获取迭代器后增加引用计数
        - 为了能够在merge iter析构时释放各memtable和version的引用计数，我们使用mem,imm_,version,3个iter以及mutex_构造IterState对象,将负责解引用的CleanupIteratorState注册进merge iter的清理函数中，负责在加锁情况下统一清理3个迭代器的引用。
    - DBImpl::Get 实现了value的访问
        - 在使用mem,imm,current_version进行Get访问时会解锁，为了防止解锁后被析构，在解锁前会增加引用计数
        - 访问完毕后会执行一次UpdateStats,更新file的seek miss字段
        - 如果存在stat_update的情况，我们调用MaybeScheduleCompactino进行基于Get事件触发的Compaction
    - DBImpl::MakeRoomForWrite 为即将到来的写入操作分配所需的空间，需要处理的情况包括:
        - makeroomforwrite首先检查level0的负载，来决定是继续插入数据还是睡眠，level0的文件储量超过8,已经触发了延迟写操作,防止level0过满,延迟写操作并不是将一个写入延长几秒,而是将几个独立的写入延迟1ms来减少延迟方差,避免写饥饿
        - memtable还有空间，直接break
        - mem已经没有空间了,且imm_不是空的,此时必须wait imm处理完毕
        - level0中的文件已经达到硬上限，必须Wait
        - 虽然mem没有空间，但是imm_还有空间，我们构造新的mem和log文件，然后将旧的mem赋值给imm_
        - 注意整个过程使用while(true)将所有上述情况的ifelse语句包裹在一起，如果**在本次循环因无法满足写入条件而等待或延时的，在等待或延时结束后，数据库状态发生变化，这时通过while再次进入if else中判断，直到可以分配空间(break）为止才可以从该函数中出来。**
    - DBImpl::Write
        - Writers_相当于一个任务队列，生产者线程不断向任务队列中添加待处理的任务。一般计算模型是将消费者和生产者分开，也就是这里另开线程处理任务。但是leveldb选择从生产者线程中找一个线程来处理任务。问题在于选择哪个生产者作为消费者线程。
        - 每个生产者在向Writers_队列中添加任务之后，都会进入一个while循环，**然后在里面睡眠**，只有当这个生产者所加入的任务位于队列的头部或者该线程加入的任务已经被处理(即writer.done == true)，线程才会被唤醒，这里需要注意，线程被唤醒后会继续检查循环条件，如果满足条件还会继续睡眠。这里分两种情况：
            - 所加入的任务被处理完毕
            - 所加入的任务排在了队列的头部
        - 对于第一种情况，线程退出循环后直接返回。对于第二种情况，leveldb将这个生产者选为消费者。然后让它进行后面的处理。为什么选择第一种情况下的生产者不作为消费者呢？**这主要是为了保证每次只有一个消费者对writers_队列进行处理**。**不管在什么情况下，只会有一个生产者线程的任务放在队列头部，但是有可能一个时间会有多个生产者线程的任务被处理掉。这保证了后续log写入和memtable写入只会有一个线程在执行，所以可以安全的进行解锁。**
    - **DB的线程同步（并行磁盘读写）**
        - leveldb在写sstable（新建sstable）时不需要加锁
            - WriteLevel0Table在BuildTable时解锁的,**也就是在从memtable写入新的sstable时可并行，只需要在改变verset状态时加锁就行**
            - DBImpl::DoCompactionWork**在实际进行压实操作时是解锁的,在解锁区的所有操作都是读操作或对CompactionState对象的写操作(写的文件都是新创建的，不存在竞争），在加锁之后才使用compactionState修改共享的versionset状态**
        - DBImpl::MakeRoomForWrite如果发现Level0层文件过多，会启动延迟写操作，延迟写操作会导致部分write处于睡眠，**在进入睡眠状态前也进行了解锁，睡眠状态结束后加锁，**使得睡眠时DB的compaction线程可以正常运行并执行压实操作，缓解level0层的负担
        - DBImpl::Get在实际读取时不需要加锁：
            - memtable的skiplist是读写安全的，不需要在读时加锁
            - imm是不可变的，不需要在读时加锁
            - sstable是不可变的，不需要在读时加锁
        - DBImpl::Write 多个并行的写着可能并行的写入memtable和log文件，如何进行同步？
        - 底层组件的线程安全和高层的线程安全到底存在什么样的联系？

- db_format
    - ParsedKey保存的是已经解码完成的Internalkey，基于ParsedKey构造了InternalKey的编码解码函数 （其设计思路类似于Blockhandle这种小的format,将编码解码器都集中在一起）
    - InternalKey
        - InternalKey保存了InternalKey的编码字符串，为该字符串提供了访问接口
        - leveldb中存储的key全部为InternalKey
        - internalKey由userkey,seq,type组成，其中在编码上，seq和type共同组成一个固定的64位数据段(8字节，这样的好处是不需要为user_key提供长度字段也可以获取user_key本身，参见ExtractUserKey)
    - InternalKeyComparator
        - 该比较器提供了对internalKey的比较规则
        - 对于userKey的比较，比较器包含了一个user_comparator_,user_comparator定义了user_key的比较规则
        - 对于seq,type字段默认进行降序比较，这样做的好处是在顺序遍历key集合时，对于user_key相同的internalkey,总是遍历到序列号最新的internalkey
    - lookupkey,如果需要将internalkey作为一个结构单独编码进文件，由于internalkey是变长的，我们需要在前面添加length,也就是lengthprefix,lookupkey提供lengthprefixinternalkey和internalkey的同时访问,lengthprefixinternalkey需要用在memtable中，而一般的internalkey则用在block中的data entry中，data entry中的key已经有entry头部的固定字节指示大小，不需要自己额外添加长度标识

- snapshot
    - **当插入数据时，SequenceNumber会依次增长**，例如插入key1, key2, key3, key4等数据时，依次对应的SequenceNumber为1, 2, 3, 4 (writebatch也是如此，MemTableInserter在写入key和value以后会将seq++)
    - snapshots_为一个维护snapshot的双向链表。每次获取一个snapshot，就以当前的SequenceNumber(**versionset中的last_sequence_**) new一个snapshot， 并插入到双向链表中。
    - Get时，可以通过option传入snapshot参数。在Get逻辑中，**实际的seek时会跳过SequenceNumber比snapshot大的kv键对。从而保证读到的时snapshot时的值，而非后续的新值。**
    - **为了保持快照中的数据不会被删除**，在leveldb中，唯一会删除数据的地方就是compaction了，也就是DBImpl::DoCompactionWork的核心部分
    - **DBImpl::DoCompactionWork 的key drop逻辑如下**
        - 首先设置smallest_snapshot,如果当时没有设置快照就直接是verset的 lastSeq
        - 根据快照的定义，对于同一个userkey,我们要保留**最后一个seq小于等于smallest_snapshot的key,以及所有seq>smallest_snapshot的key** 比如对于如下的拥有相同user_key的internalkey写入,后缀表示序列号**：**

        ```jsx
        key0 key1 key2 snapshot3 key4 key5 snapshot6 key7 ...
        ```

        - 此时所有的key的user_key相同，存在2个snapshot,最小的snapshot的seq为3（smallest_snapshot=3),为了保证在快照设置为snapshot3时可以访问到该user_key以前的值，我们必须将key2保留下来，但是序列号为0和1的key就可以直接丢弃,因为对于snapshot3来说只需要key2状态的user_key.
        - 对于上述情况，DOComapctionWork函数使用InternalkeyComparator遍历Compaction类的input key（这样可以保证对于相同的user_key最先遍历到的总是最新的seq），记录上一个key的seq(last_seq),若在user_key相同的情况下last_seq小于smallest_snapshot,说明上一个已经保留的key就是key2(**也就是最后一个快照中的数据**),当前key2就可以删除了，因为没有snapshot可以访问到该旧数据
        - 如果没有快照，smallest_snapshot=last_seq，对于如下user_key相同的internal_key:

        ```jsx
        key0 key1 key2 key3 last_seq=3
        ```

        - 此时使用DOCompactionWork函数的逻辑，key3（也就是最新的数据）会被保留,符合要求
        - 对于deletetype的internalKey存在特例,一般情况下和上面逻辑一样，如果遇到delete key且该key本身的seq≤smallest_snapshot且该key的高层没有相同的key,该key也是可以被提前drop,不需要被后续相同user_key覆盖该key时才drop。
        - DBImpl::Write 的多线程安全是如何保证的

    - iter在SeekTable::iter会时会自动跳过太大的seq,保证遍历到的都是当前快照的entry

    讲解的时候从leveldb的读写操作扩展开:

    leveldb的写操作

    leveldb的读操作涉及到读imm和读mm：

    对于imm和mm的读取，需要在临界区先**获取二者的指针(注意这个锁是保护指针的)**，然后Ref,防止二者在读取时被析构,

    也需要获取当前版本的指针(**也要加锁保护指针**），并Ref(防止析构),然后解锁,在解锁区域读取mm和imm,这里可以保证读取的安全，因为mm本身的读取是线程安全的,imm的内容是不可变的，current version的files数据在创建时就固定下来，是不可变的（不会存在version的files中途被修改的情况，LogAndApply不会修改当前version的files，而是基于currentversion添加新version）

    在获取了ref，保证了imm,mm,current version不被析构以及读取时的线程安全后，我们就可以在临界区外进行读取(Get)操作。

    快照读的实现:

    读取顺序mm→imm→level0→level1→...,按照interkey的比较规则，在key相同的情况下会定位到文件中第一个小于等于sequence的entry,实现了快照读

    由于按层顺序读取的原因怎么保证不会读取到太旧的entry,比如entry1 entry2 entry3 | entry4 entry5 的key都相同，1，2，3在同一个文件中，4，5在下一个文件中，如果压实操作只处理1，2，3，会导致get读取到4，5，无法读取到新的数据！！！

    leveldb采取的解决方法是使用AddBoundaryInputs函数提取Inputs[0]的**最大key**,使用FindSmallestBoundaryFile检测该key是否存在跨边界的现象，并返回跨边界的最小文件添加到inputs中，然后更新最大key并重复上述操作，直到不出现key的跨边界现象(lagestkey==file.smallestkey&&smallestkey.seq>largestkey.seq),这样我们避免遗漏第一层compaction中右侧的重复key（让拥有旧seq的重复key entry一定会压实到下一层），左侧的重复key不用担心，因为左侧的重复数据是最新的。

    1 2 3 | 4 5 6 7 8 | 9     ←中间的文件如果被选为input[0]的第一个file

    a a a | a a a b b | b         右边的文件会在addBoundaryInput中被包含，如果还出现跨边界重复则还会继续

    压实过程中旧数据的丢弃,docompaction在遍历前会设置好smallest_seq(根据seqList或verset的lastseq)，key相同的entry总是相邻的（使用了聚集+level迭代器），也就是说drop entry只需要观察当前遍历到的key的seq是否小于smallest_seq,如果小于seq，且last_key==当前key,该key就可以被drop,这就删除了重复的且不被快照记录的项目

    leveldb的压实操作由读(Get)写(MakeRoomForWrite)相关操作调用MaybeScheduleCompaction触发,将BGWork函数通过阻塞队列交给一个背景线程执行。如果压实操作正在背景线程中运行就不做任何操作（保持单线程compaction）,其中MaybeScheduleCompaction完成了压实操作并设置好background_compaction_scheduled_ = false;后会再次触发MaybeScheduleCompaction,看看是否有必要再次进行压实操作。

    多用户线程，单个Compaction线程

    压实操作会进入临界区，首先查看imm是否非空，如果非空优先执行CompactMemTable,如果imm为空，就看看is_manual是否打开，如果打开就使用versionset的CompactRange生成Compaction对象，否则就调用PickCompaction来生成Compaction,PickCompaction size_compaction或seek_compaction,优先进行size_compaction,如果有一个层的compaction_score_≥1就对该层进行压实操作, versionset为每一层维护一个compact_pointer_,阿里什么时候开始进系统阿里什么时候开始进系统

    leveldb的写操作:

    Build的Apply中深拷贝了一份FileMetaData，为什么要这样做？

    FileMetaData不负责表示打开文件，而是负责表示磁盘中的文件

    管理打开文件资源的是tablecache类中的cache指针,保存了TableAndFile结构，所有打开的文件都要进入cache中管理

    内存模型:

    在多线程中读取**与状态相关的标志位时**需要使用std::memory_order_acquire,来保证标志位被设置之前的逻辑都已经执行完毕

    ```c
    shutting_down_.load(std::memory_order_acquire)
    ```


