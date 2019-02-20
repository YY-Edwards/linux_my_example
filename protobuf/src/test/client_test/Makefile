CC = gcc
CXX = g++

#需要的头文件路径
INC += \
-I $(MUDUOINC) \
-I $(OBJSDIR)/../src \
-I $(OBJSDIR)/../src/codec \
-I $(OBJSDIR)/../src/dispatcher \
-I /usr/include/


CXXFLAGS +=  -std=c++11 -g

#静态模式规则
#所有的.c文件
#模式字符串替换函数——patsubst。
#$(patsubst <pattern>,<replacement>,<text> )
#查找<text>中的单词（单词以“空格”、“Tab”或“回车”“换行”分隔）是否符合模式<pattern>，如果匹配的话，则以<replacement>替换

#使用“$(wildcard *.c)”来获取工作目录下的所有的.c文件列表
CC_FILES = $(wildcard *.c)
CC_OBJS = $(patsubst %.c, %.o, $(CC_FILES))

#所有的.c++文件
CXX_FILES = $(wildcard *.cpp)
CXX_OBJS = $(patsubst %.cpp, %.o, $(CXX_FILES))

all:$(CC_OBJS) $(CXX_OBJS)
$(CC_OBJS):%.o:%.c
	$(CC) -c $< -o $(OBJSDIR_TEST_CLIENT)/$@  $(INC)  $(CCFLAGS) 
$(CXX_OBJS):%.o:%.cpp
	$(CXX) -c $< -o $(OBJSDIR_TEST_CLIENT)/$@  $(INC)  $(CXXFLAGS) 
clean:
	$(RM) $(CC_OBJS)
	$(RM) $(CXX_OBJS)
