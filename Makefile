.PHONY: all install run test clean fmt

CXX := g++
CXXFLAGS += -O2 -std=c++20 -Wall -Wextra -I src
LIBS := libevdev dbus-1
CXXFLAGS += $(shell pkg-config --cflags $(LIBS))
LDLIBS += $(shell pkg-config --libs $(LIBS))

SRCS := src/argparse.cpp \
        src/config.cpp \
        src/keymapper.cpp

HDRS := src/argparse.h \
        src/bitset.h \
        src/config.h \
        src/def.h \
        src/defer.h \
        src/kb.h \
        src/keymapper.h \
        src/log.h \
        src/test.h \
        src/window.h

OUT_DIR := out

TARGET := $(OUT_DIR)/kwin-keymapper

TEST_TARGET := $(OUT_DIR)/test

$(TARGET): $(HDRS) $(SRCS) src/main.cpp
	$(CXX) $(SRCS) $(CXXFLAGS) src/main.cpp -o $(TARGET) $(LDLIBS)

$(TEST_TARGET): $(HDRS) $(SRCS) src/test.cpp
	$(CXX) $(SRCS) $(CXXFLAGS) -DTEST src/test.cpp -o $(TEST_TARGET) $(LDLIBS)

DEVICE=

install: $(TARGET)
	@[ -c "$(DEVICE)" ] || { echo "error: DEVICE=\"$(DEVICE)\" is not a valid character device file"; exit 1;}
	@[ "$$(id -u)" -ne 0 ] || { echo "error: don't run 'make install' as root" >&2; exit 1; }
	@echo "[1/4]: Installing kwin-keymapper binary to /usr/local/bin"
	sudo cp $(TARGET) /usr/local/bin
	@echo "[2/4]: Installing KWin script"
	kpackagetool6 --type=KWin/Script --upgrade src/kwin || kpackagetool6 --type=KWin/Script --install src/kwin
	@echo "[3/4]: Installing Systemd service"
	sed -e 's|{{DBUS_ADDR}}|unix:path=/run/user/$(shell id -u)/bus|g' \
	    -e 's|{{DEVICE}}|$(DEVICE)|g'                                 \
	    src/systemd/kwin-keymapper.service                            \
	    | sudo tee /etc/systemd/system/kwin-keymapper.service > /dev/null
	@echo "[4/4]: Enable Systemd service"
	sudo systemctl daemon-reload
	sudo systemctl enable --now kwin-keymapper.service
	sudo systemctl restart kwin-keymapper.service
	@printf "Next steps (see README.md):\n- Enabling KWin script in System Settings\n- Allowing root user to access session DBus\n"

run: $(TARGET)
	./$(TARGET)

test: $(TEST_TARGET)
	./$(TEST_TARGET)

fmt:
	find src/ -type f \( -name "*.cpp" -o -name "*.h" -o -name "*.hpp" -o -name "*.cc" \) | xargs clang-format -i

clean:
	git clean -fdx $(OUT_DIR)

all: $(TARGET) $(TEST_TARGET)

