# 编译器和编译选项
CXX = g++
CXXFLAGS = -g -O0  -I/usr/local/muduo/include

# 库路径
LIBS = -lmysqlclient -lssl -lcrypto -lpthread -lmuduo_base

# 源文件和目标文件
#objects = src/main.o  src/cglmysql/sql_connection_pool.o  src/CoroutineLibrary/fd_manager.o  src/CoroutineLibrary/fiber.o  src/CoroutineLibrary/hook.o   src/CoroutineLibrary/timer.o  src/CoroutineLibrary/scheduler.o  src/CoroutineLibrary/ioscheduler.o  src/CoroutineLibrary/thread.o  src/server/tcp_server.o  src/server/http_server.o  src/server/http_conn.o     src/util/address.o  src/util/config.o  src/util/socket.o
src_dirs := src src/cglmysql src/CoroutineLibrary src/server src/util
#foreaach遍历每一个src指定子集目录下的.cc以及.cpp文件，并将.o文件重定向输出到build
objects := $(patsubst %.cpp, build/%.o, $(wildcard $(foreach dir, $(src_dirs), $(dir)/*.cpp))) \
           $(patsubst %.cc, build/%.o, $(wildcard $(foreach dir, $(src_dirs), $(dir)/*.cc)))
# 最终目标文件
edit: $(objects)
	$(CXX) $(CXXFLAGS) -o bin/CoroutineServer $(objects) $(LIBS)

# 自动创建 build 目录，并保留子目录结构
build/%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

build/%.o: %.cc
	$(CXX) $(CXXFLAGS) -c $< -o $@


main.o :   src/util/config.h  src/server/http_server.h
	$(CXX) $(CXXFLAGS) -c  src/main.cpp

sql_connection_pool.o : src/util/locker.h
	$(CXX) $(CXXFLAGS) -c src/cglmysql/sql_connection_pool.cpp

fd_manager.o : src/CoroutineLibrary/thread.h
	$(CXX) $(CXXFLAGS) -c src/CoroutineLibrary/fd_manager.cpp

fiber.o :
	$(CXX) $(CXXFLAGS) -c src/CoroutineLibrary/fiber.cpp

hook.o :
	$(CXX) $(CXXFLAGS) -c src/CoroutineLibrary/hook.cpp

scheduler.o : src/CoroutineLibrary/hook.h  src/CoroutineLibrary/fiber.h  src/CoroutineLibrary/thread.h
	$(CXX) $(CXXFLAGS) -c src/CoroutineLibrary/scheduler.cpp

timer.o :
	$(CXX) $(CXXFLAGS) -c src/CoroutineLibrary/timer.cpp

ioscheduler.o : src/CoroutineLibrary/timer.h  src/CoroutineLibrary/scheduler.h  src/CoroutineLibrary/ioscheduler.h
	$(CXX) $(CXXFLAGS) -c  src/CoroutineLibrary/ioscheduler.cpp



thread.o :
	$(CXX) $(CXXFLAGS) -c  src/CoroutineLibrary/thread.cpp
tcp_server.o :    src/server/tcp_server.h  src/util/socket.h  src/util/noncopyable.h  src/CoroutineLibrary/ioscheduler.h
	$(CXX) $(CXXFLAGS) -c  src/server/tcp_server.cc


http_server.o : src/server/tcp_server.h
	$(CXX) $(CXXFLAGS) -c  src/server/http_server.cpp

http_conn.o :    src/memorypool/memorypool.hpp  src/proxy/proxy.hpp  src/my_stl/my_stl.hpp  src/cglmysql/sql_connection_pool.h
	$(CXX) $(CXXFLAGS) -c  src/server/http_conn.cpp



address.o : util/endian.h
	$(CXX) $(CXXFLAGS) -c util/address.cc

config.o :
	$(CXX) $(CXXFLAGS) -c util/config.cpp

socket.o : util/address.h  util/noncopyable.h
	$(CXX) $(CXXFLAGS) -c util/socket.cc




# 清理目标文件
.PHONY: clean
clean:
# 删除生成的可执行文件和目标文件
	rm -f bin/CoroutineServer $(objects)
# 删除build目录下所有子目录下的.o文件
	rm -f build/**/*.o



