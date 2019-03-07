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
#所有的.c文件
CC_FILES	= $(wildcard *.c)
CC_OBJS	 	= $(patsubst %.c, %.o, $(CC_FILES))
CC_DEP 		= $(patsubst %.c, %.d, $(CC_FILES))

#所有的.c++文件
CXX_FILES 	= $(wildcard *.cpp)
CXX_OBJS 	= $(patsubst %.cpp, %.o, $(CXX_FILES))
CXX_DEP 	= $(patsubst %.cpp, %.d, $(CXX_FILES))

#Makefile 文件中的特殊变量“VPATH”就是完成这个功能的，如果没有
#指明这个变量，make 只会在当前的目录中去找寻依赖文件和目标文
#件。如果定义了这个变量，那么，make 就会在当前目录找不到的
#情况下，到所指定的目录中去找寻文件了。
#这样避免重复编译
vpath %.o $(OBJSDIR_TEST_CLIENT)

#参考方式
#strip去掉空格
#OBJ_PATH = $(OBJSDIR_TEST_CLIENT)/$(strip $(CXX_OBJS))

#静态模式规则
#<targets ...>: <target-pattern>: <prereq-patterns ...>

#“%.o”表明要所有以“.o”结尾的目标，变量$obj集合的模式；
#依赖模式“%.c”则取模式“%.o”的“%”，也就是$obj，并
#为其加下“.c”的后缀，于是，我们的依赖目标就是“$obj.c”;

#而命令中的“$<”和“$@”则是自动化变量，“$<”表示所有的依赖目标集(即是*.c或者*.cpp)
#“$@”表示目标集（*.o）



all:$(CC_OBJS) $(CXX_OBJS) $(CXX_DEP) $(CC_DEP)

$(CC_OBJS):%.o:%.c
	$(CC) -c $< -o $(OBJSDIR_TEST_CLIENT)/$@  $(INC)  $(CCFLAGS) 
$(CC_DEP):%.d:%.c
	@set -e; rm -f $@; \
	$(CXX) -MM $(CCFLAGS) $(INC) $< > $@.$$$$; \
	sed 's,\($*\)\.o[ :]*,\1.o $@ : ,g' < $@.$$$$ > $@; \
	rm -f $@.$$$$	
	
$(CXX_OBJS):%.o:%.cpp
#$(notdir $(OBJ_PATH)):%.o:%.cpp
	$(CXX) -c $< -o $(OBJSDIR_TEST_CLIENT)/$@  $(INC)  $(CXXFLAGS) 
	
$(CXX_DEP):%.d:%.cpp
	@set -e; rm -f $@; \
	$(CXX) -MM $(CXXFLAGS) $(INC) $< > $@.$$$$; \
	sed 's,\($*\)\.o[ :]*,\1.o $@ : ,g' < $@.$$$$ > $@; \
	rm -f $@.$$$$
	

-include $(CC_DEP)		
-include $(CXX_DEP)	
	
clean:
	-$(RM) $(CC_OBJS)
	-$(RM) $(CXX_OBJS)
	-$(RM) $(CXX_DEP)
	-$(RM) $(CC_DEP)

