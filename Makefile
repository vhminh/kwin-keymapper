.PHONY: all install run clean

CXX := g++
CXXFLAGS += -O2 -std=c++20 -Wall -Wextra -I src
LIBS := libevdev dbus-1
CXXFLAGS += $(shell pkg-config --cflags $(LIBS))
LDLIBS += $(shell pkg-config --libs $(LIBS))

SRCS := src/argparse.cpp \
        src/config.cpp \
        src/intercept.cpp \
        src/main.cpp

HDRS := src/argparse.h \
        src/config.h \
        src/defer.h \
        src/intercept.h \
        src/kb.h \
        src/log.h \
        src/window.h

TARGET := kwin-keymapper

$(TARGET): $(HDRS) $(SRCS)
	$(CXX) $(SRCS) $(CXXFLAGS) -o $(TARGET) $(LDLIBS)

install:
	kpackagetool6 --type=KWin/Script -i src/kwin

run: $(TARGET)
	./$(TARGET)

clean:
	rm -f $(TARGET)

all: $(TARGET)

