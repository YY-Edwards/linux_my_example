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
TARGET_CODEC_TEST 		= codec_test_app
TARGET_DISPATCHER_TEST 	= dispatcher_test_app
#TARGET = tcpreplay_app

export OBJSDIR = $(shell pwd)/objs
export OBJSDIR_CODEC = $(OBJSDIR)/codec
export OBJSDIR_DISPATCHER = $(OBJSDIR)/dispatcher
export ASDF = $(shell pwd)/bina
export MUDUOINC = $(MUDUO_INCLUDE)


#需要的库文件路径和优化参数
#-std=c++11,需要链接支持C++11的静态库
#protobuf 特殊标志：`pkg-config --cflags --libs protobuf`
##include <zlib.h> //adler32 需要连接库：-lz
LDFLAGS +=  \
-L$(MUDUO_LIBRARY) \
-L/usr/local/lib/   \
-L/usr/lib/  \
-lmuduo_net -lmuduo_base -pthread -lrt \
-lz \
`pkg-config --cflags --libs protobuf`

#v1.1.0
#-lmuduo_net_cpp11 -lmuduo_base_cpp11 -pthread -lrt

.PHONY : all
all: $(TARGET_CODEC_TEST) $(TARGET_DISPATCHER_TEST) 


$(TARGET_CODEC_TEST):$(OBJSDIR_CODEC) $(ASDF)
	
	$(MAKE) -C src/codec
	$(MAKE) -C src
	$(CXX) $(CXXFLAGS) $(OBJSDIR)/*.o $(OBJSDIR_CODEC)/*.o  -o $(ASDF)/$(TARGET_CODEC_TEST) $(LDFLAGS)
	
$(TARGET_DISPATCHER_TEST):$(OBJSDIR_DISPATCHER) $(ASDF)
	
	$(MAKE) -C src/dispatcher
	#$(MAKE) -C src	
	$(CXX) $(CXXFLAGS) $(OBJSDIR)/*.o $(OBJSDIR_DISPATCHER)/*.o  -o $(ASDF)/$(TARGET_DISPATCHER_TEST) $(LDFLAGS)	
	
$(OBJSDIR_CODEC):
	mkdir -p $@
	
$(OBJSDIR_DISPATCHER):
	mkdir -p $@	
	
$(ASDF):
	mkdir -p $@

.PHONY:clean	
clean:
	-$(RM) $(foreach dir, $(OBJSDIR_CODEC), $(wildcard $(dir)/*.o))
	-$(RM) $(foreach dir, $(OBJSDIR_DISPATCHER), $(wildcard $(dir)/*.o))
	-$(RM) $(foreach dir, $(OBJSDIR), $(wildcard $(dir)/*.o))
	-$(RM) -rf objs
	-$(RM) -rf bina
info:
	@echo "OBJSDIR:$(OBJSDIR)"
