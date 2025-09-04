CXX := g++
CXXFLAGS := -Wall -g -std=c++17
LDFLAGS := -pthread -lmysqlclient

# 源码目录
SRC_DIRS := buffer log sqlpool threadpool timer http server
MAIN_SRC := main.cpp
# 目标目录
BIN_DIR := bin

# 所有的 cpp 源文件
SRCS := $(foreach dir,$(SRC_DIRS),$(wildcard $(dir)/*.cpp))
# 生成对应的 obj 文件（放在同目录下）
OBJS := $(SRCS:.cpp=.o)
MAIN_OBJ := $(MAIN_SRC:.cpp=.o)

# 专门处理 test 目录下的 cpp -> 对应的可执行文件名
# TESTS := $(notdir $(basename $(wildcard test/*.cpp)))

# bin 下的可执行文件
# TARGETS := $(addprefix $(BIN_DIR)/,$(TESTS))
TARGET := $(BIN_DIR)/webserver

.PHONY: all clean # $(TESTS)

# 默认编译所有 test
all: $(TARGET)

# 规则：make test_xxx 直接生成 bin/test_xxx
# $(TESTS): %: $(BIN_DIR)/%
#	@echo "Build finished: $@"

# 链接主程序
$(TARGET): $(MAIN_OBJ) $(OBJS) | $(BIN_DIR)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDFLAGS)

# 链接规则：bin/test_xxx
# $(BIN_DIR)/%: test/%.cpp $(filter-out test/%.o,$(OBJS)) | $(BIN_DIR)
# 	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDFLAGS)

# 编译 cpp -> o
%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

# 确保 bin 目录存在
$(BIN_DIR):
	mkdir -p $(BIN_DIR)

clean:
	rm -rf $(OBJS) $(BIN_DIR)
