# cmake 最低版本号要求
cmake_minimum_required(VERSION 2.8)

# 设置项目名和版本号
project(leveldb_test)
# 指定C++标准
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED True)
set(CMAKE_BUILD_TYPE "Debug")

# 查找当前目录下的所有源文件
# 并将名称保存到 DIR_SRCS 变量
aux_source_directory(. DIR_SRCS)

# 添加第三方库的头文件搜索目录,这样就可以使用#include<>来在搜索目录中寻找头文件
# boost库头文件无法自动提示
include_directories(/home/yszc/Projects/cxx_projects/learn_db/leveldb/include)

# 添加第三方库的库文件搜索目录,注意只是添加了搜索目录(不是把该目录中的所有库文件都链接到main中),这样在使用target_link_libraries添加链接库时可以直接制定库名而不需要输入全路径
# link_directories()

# 这里采用的方法是将muduo库目录下所有的库文件路径都保存到一个变量中,全部使用target_link_libraries添加到程序中
# set(MUDUO_LIB_DIR /home/yszc/Projects/cxx_projects/learn_np/muduo_build/release-install-cpp11/lib)
# file(GLOB MUDUO_LIBS ${MUDUO_LIB_DIR}/*.a)

# 这里包含了库文件夹的路径,方便我们在链接库时写库名
link_directories(/home/yszc/Projects/cxx_projects/learn_db/leveldb/build)

# 指定编译包含的文件 
add_executable(main ${DIR_SRCS})

# 这里才是真正链接库文件的地方,所有输入的库以库的绝对路径或库名称表示(库名称会在之前添加的库搜索目录中查找),MUDUO_LIBS实际上是绝对路径的集合
# 就算库文件在/usr/lib/(link_directories)里,不在此链接也无法编译通过
# 动态库(.a)和静态库(.so)都可以链接
# 链接顺序:A 依赖 B,则A应该放在B左边
# main 依赖 libmuduo_net和libmuduo_base, libmuduo_net 依赖 libmuduo_base,因此有下面的依赖顺序,为了方便写库名,我们使用link_directories添加库路径
target_link_libraries(main libleveldb.a -lpthread -lsnappy)

# YCM补全
set( CMAKE_EXPORT_COMPILE_COMMANDS ON )


# 如果在内核库函数头文件中不用extern“C”来声明库函数的话，在编写C++应用程序时，包含库头文件，在C++文件链接时就会以C++标准来链接库的函数名，
# 而在库文件实现时是用C来实现的，二者函数名不同，在链接时就会出现找不到函数的现象。
# undefined reference to `muduo::CurrentThread::t_cachedTid' 中undefined表示你根本就没有链接头文件
# 出现错误先用clang调试,调好了再用cmake
# 库的链接顺序不当也会导致undefine reference error! https://www.jianshu.com/p/e6854492e401
# boost库和stl库无法自动补全,且cmake无法让/usr/include添加到compile_commands中,所以直接修改compile_commands.json文件,来让ycm补全回复,另外把cmm命令和cmmrun命令的cp compile_commands取消,来防止文件被覆盖
# 改进上述方法,使用python的json库将include path 写到compile_commands.json中
# 如果编译多个文件,会在compiles_json中生成针对多个cc文件的flag,导致在编辑head文件时出现无法补全的问题,这时把除了main文件以外的其他flag全部删除,或者使用add_path.py向每个编译单元的flag中添加默认路径
