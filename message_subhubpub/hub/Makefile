CXX = g++
CC = gcc

#使用原始库
#MUDUO_DIRECTORY = /opt/muduo_lib_file/build/release-install
#MUDUO_INCLUDE = $(MUDUO_DIRECTORY)/include
#MUDUO_LIBRARY = $(MUDUO_DIRECTORY)/lib


#使用自定义库
MUDUO_DIRECTORY = $(shell pwd)/lib/muduo_lib
MUDUO_INCLUDE = $(MUDUO_DIRECTORY)/include
MUDUO_LIBRARY = $(MUDUO_DIRECTORY)/lib

#表示用于C++编译器的选项 
CXXFLAGS +=  -g -O0 -Wall -Wextra -Werror -Wl,--as-needed -std=c++11
TARGET = hubserver_app
export OBJSDIR = $(shell pwd)/objs
export ASDF = $(shell pwd)/bina
export MUDUOINC = $(MUDUO_INCLUDE)


#需要的库文件路径和优化参数
#-std=c++11,需要连接支持C++11的静态库
LDFLAGS +=  \
-L$(MUDUO_LIBRARY) \
-L/usr/local/lib/   \
-L/usr/lib/  \
-lmuduo_net_cpp11 -lmuduo_base_cpp11 -pthread -lrt

$(TARGET):$(OBJSDIR) $(ASDF)
	
	$(MAKE) -C src
	$(CXX) $(CXXFLAGS) $(OBJSDIR)/*.o  -o $(ASDF)/$(TARGET) $(LDFLAGS)
$(OBJSDIR):
	mkdir -p $@
$(ASDF):
	mkdir -p $@

clean:
	-$(RM) $(TARGET)
	-$(RM) $(foreach dir, $(OBJSDIR), $(wildcard $(dir)/*.o))
	-$(RM) -rf objs
	-$(RM) -rf bin
info:
	@echo "OBJSDIR:$(OBJSDIR)"
