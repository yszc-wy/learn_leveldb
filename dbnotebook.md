## 数据库概论笔记
- 历年磁盘内存访问延迟对比 https://colin-scott.github.io/personal_website/research/interactive_latency.html
- 一个长期对外运行的数据库是如何处理内存碎片等长期积累的问题,加快运行速度的?,是fallover还是运行时动态清理?
- linux的tast_struct结构中有大量的list_head结构,用于构建tast_struct的链接网络,表示该tast_struct是多个内核链表结构的成员(thread_group,thread_node,tasks,sibling),或者 task包含了一个链表(children子进程链表, **该结构的next,pre指向sibling ,也就是进程的兄弟节点链表**)
- list_head加快了遍历链表的速度,在遍历时省去了在类内不断定位next指针的时间消耗,如果想要获取struct的指针需要用偏移量计算即可,这也要求list_head之前的数据成员大小固定,list_head这种结构能以相同的形式与不同的struct结合,非常灵活
- LSM以磁盘顺序写代替磁盘随机写,略微降低读性能(可以优化)
- LSM内存结构有点双缓冲的味道(muduo日志系统?)
- 从memtable(跳表)开始构造的有序表结构从内存开始一层层向下(磁盘)传递,只有active memtable可被修改(可以保证单个表内的key不重),传递到下层就不变了.
- LSM https://www.cnblogs.com/Finley/p/13900987.html
- LSM的读取也是从上层开始一步一步向下查找,最先找到的就是最新的.LSM读取的速度理论上低于B树,但这个读取操作有很多可优化的地方(表内索引,bloomFilter,内存中的快缓存和表缓存等)
- 为了删除旧数据,必须对SSTable进行合并和压缩(Compact)
- 读放大:读取数据时实际读取的数据量大于真正的数据量。例如 LSM 读取数据时需要扫描多个 SSTable.
- 写放大:写入数据时实际写入的数据量大于真正的数据量。例如在 LSM 树中写入时可能触发Compact操作，导致实际写入的数据量远大于该key的数据量。
- 空间放大:数据实际占用的磁盘空间比数据的真正大小更多。例如上文提到的 SSTable 中存储的旧版数据都是无效的。
- 使用上述3个策略来评判Compact算法
- Size Tiered Compaction Strategy(STCS) 同一层会存在重复元素,读放大,空间放大严重
  - 会产生表饥饿,就是某一层在Compaction之后不足以作为下一层的一个sstable,导致下一层产生Compaction,下一层中重复的元素和墓碑一直不被回收,增加了读取的开销
- Leveled Compaction Strategy(LCS) 使用上一层的一个sstable有序表的范围与下一层指定范围的几个sstable做归并操作,使得每一层不存在重复元素,也让所有层的表都是有序的,降低了读放大,空间放大
  - RocksDB 的Level0层是由memtable刷写出来的,由于热点key的存在,Level0层极容易出现重复key,为了防止每个memetable刷到level1都要进行合并操作,level0允许重复,且在文件个数超过阈值后触发压缩操作，所有的 L0 文件都将被合并进L1
- 二者的trade-off Leveled-N
- 由于sstable的不可变性(内容一旦写入就无法修改其中的一部分,除非直接替换),使得读快,更新复杂的顺序结构、B树、hash索引优化都可以放心的使用到sstable中,而无需担心这些结构的负面影响(内存碎片,写放大啥的)
- 为读做的优化可能导致更新操作的复杂,比如指针的加入导致结构更新时指针也要更新
- Bloom Filter使用bit数组和多个hash函数来判断一个key可能存在/不存在于一个集合,当key经过hash函数所定位的所有bit位为1,说明可能存在,若至少有一个为0,说明肯定不存在.可能会存在不同key经hash函数返回相同位置的情况(hash冲突),可以通过扩充bit位数量和hash函数数量来减少冲突,当然bit位越多空间负担越大,hash函数越多性能负担越大,需要trade-off,可以根据预期的集合大小来判断假阳性的概率
- 概率型数据结构通常比对应的常规数据结构具有更高的空间效率,如Bloom Filter,HyperLogLog,CountMin Sketch
- SSD垃圾回收机制 https://www.zhihu.com/question/31024021
- LSM将原来小的随机写入缓冲为批量操作,减少了写入操作的次数,进而减少了SSD垃圾收集的次数,减少了写放大的情况
- Leveldb为了保证每次合并时不会向下层重叠太多sstable,还要求合并输出的sstable的key范围不超过下层的10个sstable文件
- leveldb压实操作时对sstable的选择还遵从旋转规则,对level l的压实操作会记录压实后的endkey,下一次选择sstable时会选择该key之后的第一个sstable
- leveldb的压实操作与写入操作时间差异问题,若压实操作所需时间大于写入操作时间,会导致大量文件在level0处堆积,造成读取操作的额外性能损失(level0上可能有很多重复的key)
- 每次写都用同步是不现实的,leveldb在异步模式下可以采用每写N次就fsync一次的方法来保证crash时上一个fsync执行点之后开始bulk load恢复,或者在writebatch时使用fsync来让每个写操作平摊fsync消耗的时间
- 线程安全性分析: https://clang.llvm.org/docs/ThreadSafetyAnalysis.html
- 虚拟地址的本质是将不同的对象(物理内存、磁盘文件)映射到不同类型的vm_area_struct(虚拟内存区域)中,进程间的共享代码段其实就是共享同一个可读、可执行、不可写的磁盘文件(库文件)映射 Linux内核 P255
- 虚拟地址到物理地址的映射见页表,机组或操作系统
- 竞争条件: 如果2个函数同时运行,系统用各种时序对其调度的总体结果(注意是总体,不是单个函数的差异,比如fun1,fun2用不同的顺序对i做加法,加法的结果肯定不一样,fun1先的话,fun1的结果是1,fun1后的话,fun1的结果是2)存在差异,说明这2个函数存在竞争条件,如:
  对于全局变量i
  fun1{i+=1}  fun2(i+=1){i+=1}
  系统可能采用三种方式调度:
  1. 先运行fun1,fun1代码执行完毕后再执行fun2 总体+2
  2. 先运行fun2,fun2代码执行完毕后再执行fun1 总体+2
  3. fun1,fun2代码交叉执行 可能出现总体+1
  结果不一样,存在条件竞争
  如果加锁,同时运行只会出现2中调度情况
  1. 先运行fun1,fun1代码执行完毕后再执行fun2 总体+2
  2. 先运行fun2,fun2代码执行完毕后再执行fun1 总体+2
  结果一样,线程安全
  对于leveldb中Read和skip的例子. ????? 不知道到底安全不安全
  1. 先运行Read后运行Skip 总体读的是Skip前的offset
  2. 先运行Skip后运行Read 总体读的是Skip后的offset
  这里就算加锁,好像结果也一样.....
  结果不一样,存在条件竞争
再多看看条件竞争的例子
- 为何使用多级页表 https://blog.csdn.net/ibless/article/details/81275009
- 页、页高速缓存、块、块I/O、文件系统、进程地址空间综合考虑
- 内核以块为单位访问磁盘,因为扇区是磁盘的最小可寻址,所以块必须比扇区大(2的整数倍),且比页面小,块也是文件系统的最小寻址单元,大小一般是512B 1KB 4KB
- 当一个块被调入内存,要存储在一个缓冲区中,每个缓冲区与一个块对应,相当于磁盘块在内存中的表示,一个页可以容纳一个或多个内存中的块(缓冲区),由于需要标识一个块在哪个块设备,对应内存中的哪个缓冲区,我们使用缓冲区头作为缓冲区的描述符
- 为了块I/O的简洁高效,使用BIO作为内核块I/O操作的基本容器,BIO以页为单位,将每个页内的缓冲区(bio_vec)通过链表串联起来,实现聚散I/O
- 块I/O请求保存在请求队列中,队列中的请求可以由多个bio结构体组成
- 页高速缓冲 address_space,包含作为高速缓冲的多个页面,这些页面由radix组织,一个文件(inode)通常和一个address_space关联,可以通过文件的offset对radix树进行检索
- address_space是vm_area_struct的物理地址对等体,一个文件只有一个address_space来保存文件的缓冲数据,但可以被多个进程的vm_area_struct所标识并共享,让多个进程可以通过虚拟地址直接访问高速缓冲区的文件数据
- 页高速缓冲也可以不和inode关联,比如和swapper关联时,host会变为null
- 读操作的步骤:
  - 查看高速缓存中是否存在需要的页,没有就分配一个新页面(由缓存回收策略管理)
  - 创建读请求从磁盘读取数据,写入到相应的页面上
  - 将缓存中的数据拷贝到用户空间
- 写操作的步骤(回写):
  - 先在页高速缓存中搜索需要的页,若没有就分配一个空闲项
  - 创建写请求(request),使用bio指定要写入到磁盘的内存缓冲区
  - 数据从用户空间拷贝到内核缓冲
  - 数据写入磁盘
- 快速从页高速缓冲中查找指定的页使用了radix-tree
- 缓冲区高速缓存与页高速缓存:
- “buffer cache”中的buffer指的是以前块设备层中用来缓存磁盘内容的结构，一个buffer大小就是磁盘中一个block的大小。
- “page cache”指的是文件系统层用于缓存读写内容的cache，因为这一层在设备层之上，因此和内核其他地方一样，以page为单位来管理。
- 由于内核仍需要根据块而不是页面执行块I/O，因此保留了缓冲区高速缓存。由于大多数块表示文件数据，因此大多数缓冲区高速缓存由页高速缓存表示。但是少量的块数据不是文件备份的（例如，元数据和原始块I / O），因此仅由缓冲区高速缓存表示。
- 在块I / O下没有文件，文件系统或目录的概念。整个磁盘或LUN只是一大堆磁盘块。在文件I/O下，一大堆磁盘块被分成小块，这些小块在逻辑上连接到表单文件和目录
- emplace_back可以避免使用参数构造对象时额外的构造操作,直接原地通过参数构造对象,如果是push_back,需要调用构造函数构造临时对象,再调用移动构造函数构造对象.
```c++
  elections.emplace_back("Nelson Mandela", "South Africa", 1994);
  reElections.push_back(President("Franklin Delano Roosevelt", "the USA", 1936));
```
- std::this_thread可以获取线程的id!!!
- std::numeric_limits<int>::max();获取int的最大值,不需要用INT32
- env_posix.cc中的阻塞队列并没有在每次插入队列是通知,只在从0～1时刻通知:
  - 该方法在有多个背景线程的情况下,例如多个backthread预先阻塞,多个schedule顺序执行(中间没有backthread进入临界区)enqueue操作,会导致signal只执行一次,导致多个schedule却只有一个工作线程被唤醒,使得该线程负担了所有enqueue插入任务的工作量(因为只有从0->1时才会notify另外一个线程),其他backthread一直处于阻塞状态(饿死),降低了线程池的效率,线程池不能使用这种方法,只能在单工作线程时使用
所幸这里只有一个线程,所以不是问题
- 关于内存对齐 https://www.zhihu.com/question/27862634
- 关于C++11的内存对齐align 
  - https://blog.csdn.net/luoshabugui/article/details/83268086
- 关于std::aligned_storage 
  - https://blog.csdn.net/qq_29426201/article/details/106056348?utm_medium=distribute.pc_relevant.none-task-blog-BlogCommendFromMachineLearnPai2-2.channel_param&depth_1-utm_source=distribute.pc_relevant.none-task-blog-BlogCommendFromMachineLearnPai2-2.channel_param
  - https://zh.cppreference.com/w/cpp/types/aligned_storage
- new https://zh.cppreference.com/w/cpp/language/new
- 内存对齐主要遵循下面三个原则:
  - 结构体变量的起始地址能够被其最宽的成员大小整除
  - 结构体每个成员相对于起始地址的偏移能够被其自身大小整除，如果不能则在前一个成员后面补充字节
  - **结构体总体大小**能够被最宽的成员的大小整除，如不能则在后面补充字节 
- std::var_list
