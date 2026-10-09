# tools/ime-test/Makefile —— 在 PC 上编译输入法引擎，不需要 ESP-IDF
#
# 这个 Makefile 能存在，全靠 app/ime 守住了「只依赖 C 标准库」那条纪律。
# 一旦有人往引擎里加了 esp_*.h，这里就会编不过 —— 那正是我们想要的早期警报。

ROOT      := ../..
IME       := $(ROOT)/app/ime
GEN       := build/gen
REGISTRY  := $(GEN)/ime_registry.c
PYTHON    ?= python

CC        ?= cc
CFLAGS    := -std=c17 -Wall -Wextra -Werror -g -O0 \
             -I$(IME) -I$(IME)/core -I$(IME)/conformance

# 引擎源码全收。--include-underscore 让 _template 也编进来：
# 模板本身必须能通过一致性测试，否则贡献者复制到的就是个坏起点。
ENGINE_SRCS := $(wildcard $(IME)/engines/*/*.c)
CORE_SRCS   := $(IME)/core/ime_core.c $(IME)/conformance/conformance.c
SRCS        := main.c $(CORE_SRCS) $(ENGINE_SRCS) $(REGISTRY)

TARGET := ime-test

.PHONY: all check clean
all: $(TARGET)

$(REGISTRY): $(wildcard $(IME)/engines/*/*.c) $(ROOT)/tools/gen_ime_registry.py
	@mkdir -p $(GEN)
	$(PYTHON) $(ROOT)/tools/gen_ime_registry.py $(IME)/engines $(REGISTRY) --include-underscore

$(TARGET): $(SRCS)
	$(CC) $(CFLAGS) -o $@ $(SRCS)

# 贡献者提 PR 前跑这个
check: $(TARGET)
	./$(TARGET) check

clean:
	rm -rf $(TARGET) $(TARGET).exe build
