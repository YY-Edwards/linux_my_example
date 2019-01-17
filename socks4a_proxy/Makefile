CXX = g++
CC = gcc

#使用原始库
#MUDUO_DIRECTORY = /opt/muduo_lib_file/build/release-install
#MUDUO_INCLUDE = $(MUDUO_DIRECTORY)/include
#MUDUO_LIBRARY = $(MUDUO_DIRECTORY)/lib


#使用自定义库
#v1.1.0
#MUDUO_DIRECTORY = $(shell pwd)/lib/muduo_lib   

#v2.0.0
MUDUO_DIRECTORY = $(shell pwd)/lib/release-install-cpp11
MUDUO_INCLUDE = $(MUDUO_DIRECTORY)/include
MUDUO_LIBRARY = $(MUDUO_DIRECTORY)/lib

#表示用于C++编译器的选项 
CXXFLAGS +=  -g -O0 -Wall -Wextra -Werror -Wl,--as-needed -std=c++11
TARGET_REPLAY = tcpreplay_app
TARGET_SOCKS4A = socks4a_app
#TARGET = tcpreplay_app

export OBJSDIR = $(shell pwd)/objs
export OBJSDIR_REPLAY = $(OBJSDIR)/tcpreplay
export OBJSDIR_SOCKS4A = $(OBJSDIR)/socks4a
export ASDF = $(shell pwd)/bina
export MUDUOINC = $(MUDUO_INCLUDE)


#需要的库文件路径和优化参数
#-std=c++11,需要链接支持C++11的静态库
LDFLAGS +=  \
-L$(MUDUO_LIBRARY) \
-L/usr/local/lib/   \
-L/usr/lib/  \
-lmuduo_net -lmuduo_base -pthread -lrt
#v1.1.0
#-lmuduo_net_cpp11 -lmuduo_base_cpp11 -pthread -lrt

all: $(TARGET_REPLAY) $(TARGET_SOCKS4A)
.PHONY : all

$(TARGET_REPLAY):$(OBJSDIR_REPLAY) $(ASDF)
	
	$(MAKE) -C src/tcpreplay
	$(MAKE) -C src
	$(CXX) $(CXXFLAGS) $(OBJSDIR)/*.o $(OBJSDIR_REPLAY)/*.o  -o $(ASDF)/$(TARGET_REPLAY) $(LDFLAGS)
	
$(TARGET_SOCKS4A):$(OBJSDIR_SOCKS4A) $(ASDF)
	
	$(MAKE) -C src/socks4a
	$(MAKE) -C src
	$(CXX) $(CXXFLAGS) $(OBJSDIR)/*.o $(OBJSDIR_SOCKS4A)/*.o  -o $(ASDF)/$(TARGET_SOCKS4A) $(LDFLAGS)	
	
$(OBJSDIR_REPLAY):
	mkdir -p $@
	
$(OBJSDIR_SOCKS4A):
	mkdir -p $@	
	
$(ASDF):
	mkdir -p $@

.PHONY:clean	
clean:
	-$(RM) $(foreach dir, $(OBJSDIR_REPLAY), $(wildcard $(dir)/*.o))
	-$(RM) $(foreach dir, $(OBJSDIR_SOCKS4A), $(wildcard $(dir)/*.o))
	-$(RM) $(foreach dir, $(OBJSDIR), $(wildcard $(dir)/*.o))
	-$(RM) -rf objs
	-$(RM) -rf bina
info:
	@echo "OBJSDIR:$(OBJSDIR)"
