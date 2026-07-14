.PHONY: all install setup run clean fmt

CXX := g++
CXXFLAGS += -O2 -std=c++20 -Wall -Wextra -I src
LIBS := libevdev dbus-1
CXXFLAGS += $(shell pkg-config --cflags $(LIBS))
LDLIBS += $(shell pkg-config --libs $(LIBS))

SRCS := src/argparse.cpp \
        src/config.cpp \
        src/intercept.cpp

HDRS := src/argparse.h \
        src/config.h \
        src/def.h \
        src/defer.h \
        src/intercept.h \
        src/kb.h \
        src/log.h \
        src/test.h \
        src/window.h

OUT_DIR := out

TARGET := $(OUT_DIR)/kwin-keymapper

TEST_TARGET := $(OUT_DIR)/test

$(TARGET): setup $(HDRS) $(SRCS) src/main.cpp
	$(CXX) $(SRCS) $(CXXFLAGS) src/main.cpp -o $(TARGET) $(LDLIBS)

$(TEST_TARGET): setup $(HDRS) $(SRCS) src/test.cpp
	$(CXX) $(SRCS) $(CXXFLAGS) -DTEST src/test.cpp -o $(TEST_TARGET) $(LDLIBS)

install:
	kpackagetool6 --type=KWin/Script -i src/kwin

setup:
	mkdir -p $(OUT_DIR)

run: setup $(TARGET)
	./$(TARGET)

test: setup $(TEST_TARGET)
	./$(TEST_TARGET)

fmt:
	find src/ -type f \( -name "*.cpp" -o -name "*.h" -o -name "*.hpp" -o -name "*.cc" \) | xargs clang-format -i

clean:
	rm -rf $(OUT_DIR)

all: $(TARGET) $(TEST_TARGET)

