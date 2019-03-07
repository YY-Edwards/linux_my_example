CXX = g++
CC = gcc

#总控Makefile的变量可以传递到下级的Makefile中（如果你显示的声明），
#但是不会覆盖下层的Makefile中所定义的变量，除非指定了“-e”参数。

#$@--目标文件，$^--所有的依赖文件，$<--第一个依赖文件。
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
TARGET_CODEC_TEST 		= codec_test
TARGET_DISPATCHER_TEST 	= dispatcher_test
TARGET_CLIENT_TEST 		= client_test
TARGET_SERVER_TEST 		= server_test
TARGET_FILEUPLOAD_CLIENT_TEST 	= file_upload_client_test
TARGET_FILEUPLOAD_SERVER_TEST 	= file_upload_server_test
#TARGET = tcpreplay_app

#需要传递到下层的变量
#unexport:仅本层有效
export SRCDIR 						= $(shell pwd)/src
export OBJSDIR 						= $(shell pwd)/objs

export OBJSDIR_TEST 				= $(OBJSDIR)/test
export OBJSDIR_TEST_CODEC 			= $(OBJSDIR_TEST)/codec
export OBJSDIR_TEST_DISPATCHER 		= $(OBJSDIR_TEST)/dispatcher
export OBJSDIR_TEST_CLIENT 			= $(OBJSDIR_TEST)/client
export OBJSDIR_TEST_SERVER 			= $(OBJSDIR_TEST)/server
export OBJSDIR_TEST_FILE_UPLOAD_C 	= $(OBJSDIR_TEST)/file_upload_client
export OBJSDIR_TEST_FILE_UPLOAD_S 	= $(OBJSDIR_TEST)/file_upload_server
export OBJSDIR_MODULE 				= $(OBJSDIR)/module
 
export ASDF 						= $(shell pwd)/bina
export MUDUOINC 					= $(MUDUO_INCLUDE)


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
#Makefile中的第一个目标会被作为其默认目标。我们声明了一个“all”的伪目标，其依赖于其它三个目标。由于伪目标的特性是，总是被执行的，所以其依赖的那三个目标就总是不如“all”这个目标新。
#所以，其它三个目标的规则总是会被决议。也就达到了我们一口气生成多个目标的目的。“.PHONY : all”声明了“all”这个目标为“伪目标”

#
#Makefile 中的第一个目标会被作为其默认目标。我们声明了一个“all”的伪目标，
#其依赖于其它三个目标。由于伪目标的特性是，总是被执行的，
#所以其依赖的那三个目标就总是不如“all”这个目标新。
#所以，其它三个目标的规则总是会被决议。也就达到了我们一口气生成多个目标的目的。
#“.PHONY : all”声明了“all”这个目标为“伪目标



.PHONY : all
all: $(TARGET_CODEC_TEST) $(TARGET_DISPATCHER_TEST) $(TARGET_CLIENT_TEST) $(TARGET_SERVER_TEST) \
$(TARGET_FILEUPLOAD_CLIENT_TEST) $(TARGET_FILEUPLOAD_SERVER_TEST) 



#目标：依赖项
# 命令
$(TARGET_CODEC_TEST):  $(OBJSDIR_TEST_CODEC) $(OBJSDIR_MODULE) $(OBJSDIR) $(ASDF)
	$(MAKE) -C src/test/codec_test
	$(MAKE) -C src/codec
	$(MAKE) -C src
	$(CXX) $(CXXFLAGS) $(OBJSDIR)/*.o $(OBJSDIR_MODULE)/*.o $(OBJSDIR_TEST_CODEC)/*.o  -o $(ASDF)/$(TARGET_CODEC_TEST) $(LDFLAGS)
	

#$(TARGET_CODEC_TEST): $(OBJSDIR_TEST_CODEC) $(OBJSDIR_MODULE) $(OBJSDIR) $(ASDF)
#$(MAKE) -C src/test/codec_test
#$(MAKE) -C src/codec
#$(MAKE) -C src
#$(CXX) $(CXXFLAGS)  $(OBJSDIR)/*.o $(OBJSDIR_MODULE)/*.o $(OBJSDIR_TEST_CODEC)/*.o  -o $(ASDF)/$(TARGET_CODEC_TEST) $(LDFLAGS)
	
$(TARGET_DISPATCHER_TEST):$(OBJSDIR_TEST_DISPATCHER) $(OBJSDIR_MODULE) $(ASDF)
	
	$(MAKE) -C src/test/dispatcher_test
	$(MAKE) -C src/dispatcher
	$(CXX) $(CXXFLAGS)  $(OBJSDIR)/*.o $(OBJSDIR_MODULE)/*.o $(OBJSDIR_TEST_DISPATCHER)/*.o  -o $(ASDF)/$(TARGET_DISPATCHER_TEST) $(LDFLAGS)	
	
$(TARGET_CLIENT_TEST):$(OBJSDIR_TEST_CLIENT) $(OBJSDIR_MODULE) $(ASDF)
	$(MAKE) -C src/test/client_test
	$(CXX) $(CXXFLAGS)  $(OBJSDIR)/*.o $(OBJSDIR_MODULE)/*.o $(OBJSDIR_TEST_CLIENT)/*.o  -o $(ASDF)/$(TARGET_CLIENT_TEST) $(LDFLAGS)	

$(TARGET_SERVER_TEST):$(OBJSDIR_TEST_SERVER) $(OBJSDIR_MODULE) $(ASDF)
	$(MAKE) -C src/test/server_test
	$(CXX) $(CXXFLAGS)  $(OBJSDIR)/*.o $(OBJSDIR_MODULE)/*.o $(OBJSDIR_TEST_SERVER)/*.o  -o $(ASDF)/$(TARGET_SERVER_TEST) $(LDFLAGS)	


$(TARGET_FILEUPLOAD_CLIENT_TEST):$(OBJSDIR_TEST_FILE_UPLOAD_C) $(OBJSDIR_MODULE) $(ASDF)
	$(MAKE) -C src/test/file_upload_test/client
	$(MAKE) -C src/file_proto_api
	$(CXX) $(CXXFLAGS)  $(OBJSDIR)/*.o $(OBJSDIR_MODULE)/*.o $(OBJSDIR_TEST_FILE_UPLOAD_C)/*.o  -o $(ASDF)/$(TARGET_FILEUPLOAD_CLIENT_TEST) $(LDFLAGS)	
	
$(TARGET_FILEUPLOAD_SERVER_TEST):$(OBJSDIR_TEST_FILE_UPLOAD_S) $(OBJSDIR_MODULE) $(ASDF)
	$(MAKE) -C src/test/file_upload_test/server
	$(MAKE) -C src/taskthreadpool
	$(MAKE) -C src/file_server
	$(MAKE) -C src/file_proto_api
	$(CXX) $(CXXFLAGS)  $(OBJSDIR)/*.o $(OBJSDIR_MODULE)/*.o $(OBJSDIR_TEST_FILE_UPLOAD_S)/*.o  -o $(ASDF)/$(TARGET_FILEUPLOAD_SERVER_TEST) $(LDFLAGS)	
	
	
	
	
	
#创建目标文件路径	
$(OBJSDIR_TEST_FILE_UPLOAD_S):
	mkdir -p $@	
	
$(OBJSDIR_TEST_FILE_UPLOAD_C):
	mkdir -p $@	

$(OBJSDIR_TEST_CLIENT):
	mkdir -p $@	
	
$(OBJSDIR_TEST_SERVER):
	mkdir -p $@	
	
$(OBJSDIR_TEST_CODEC):
	mkdir -p $@
	
$(OBJSDIR_TEST_DISPATCHER):
	mkdir -p $@	

$(OBJSDIR_MODULE):
	mkdir -p $@		

	
$(ASDF):
	mkdir -p $@

#.PHONY意思表示clean是一个“伪目标”，。
#而在rm命令前面加了一个小减号的意思就是，也许某些文件出现问题，但不要管，继续做后面的事。
#当然，clean的规则不要放在文件的开头，不然，这就会变成make的默认目标，相信谁也不愿意这样。不成文的规矩是——“clean从来都是放在文件的最后”。	

#$(foreach <var>,<list>,<text> )
#显示的说明clean是个伪目标	
.PHONY:clean	
clean:
	-$(RM) $(foreach dir, $(OBJSDIR_TEST_CLIENT), $(wildcard $(dir)/*.o))
	-$(RM) $(foreach dir, $(OBJSDIR_TEST_CODEC), $(wildcard $(dir)/*.o))
	-$(RM) $(foreach dir, $(OBJSDIR_TEST_DISPATCHER), $(wildcard $(dir)/*.o))
	-$(RM) $(foreach dir, $(OBJSDIR_TEST_SERVER), $(wildcard $(dir)/*.o))
	-$(RM) $(foreach dir, $(OBJSDIR_TEST_FILE_UPLOAD_C), $(wildcard $(dir)/*.o))
	-$(RM) $(foreach dir, $(OBJSDIR_TEST_FILE_UPLOAD_S), $(wildcard $(dir)/*.o))
	-$(RM) $(foreach dir, $(OBJSDIR_MODULE), $(wildcard $(dir)/*.o))
	-$(RM) $(foreach dir, $(OBJSDIR), $(wildcard $(dir)/*.o))
	-$(RM) -rf objs
	-$(RM) -rf bina
info:
	@echo "OBJSDIR:$(OBJSDIR)"
