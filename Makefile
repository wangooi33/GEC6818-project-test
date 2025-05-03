# 编译器
CC = arm-linux-gcc

# 编译选项
CFLAGS = -I./include

# 链接库路径
LDFLAGS = -L. -L./lib

# 要链接的库
LIBS = -lfont -lfont_old -ljpeg -lpthread -lm

# 源文件和目标文件
SRC = main.c
TARGET = main.out

# 默认目标
all: $(TARGET)

# 生成目标程序
$(TARGET): $(SRC)
	$(CC) $(SRC) $(CFLAGS) $(LDFLAGS) $(LIBS) -o $(TARGET)

# 清理编译产生的文件
clean:
	rm -f $(TARGET)
