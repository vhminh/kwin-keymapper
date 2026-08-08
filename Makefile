.PHONY: all install run test clean fmt

SRC_DIR := src
HDR_DIR := src
OUT_DIR := out
TARGET := $(OUT_DIR)/kwin-keymapper
TEST_TARGET := $(OUT_DIR)/test

SRCS := argparse.cpp \
        config.cpp \
        keymapper.cpp \
        stats.cpp

CXX := g++
CXXFLAGS += -O2 -std=c++20 -Wall -Wextra -MMD -MP -I $(HDR_DIR)
LIBS := libevdev dbus-1
CXXFLAGS += $(shell pkg-config --cflags $(LIBS))
LDFLAGS += $(shell pkg-config --libs $(LIBS))

OBJ_DIR := $(OUT_DIR)/main-obj
OBJS := $(SRCS:%.cpp=$(OBJ_DIR)/%.o) $(OBJ_DIR)/main.o
TEST_OBJ_DIR := $(OUT_DIR)/test-obj
TEST_OBJS := $(SRCS:%.cpp=$(TEST_OBJ_DIR)/%.o) $(TEST_OBJ_DIR)/test.o

DEPS := $(OBJS:.o=.d)
DEPS += $(TEST_OBJS:.o=.d)

$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) $(OBJS) -o $@ $(LDFLAGS)

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(TEST_TARGET): $(TEST_OBJS)
	$(CXX) $(CXXFLAGS) $(TEST_OBJS) -o $@ $(LDFLAGS)

$(TEST_OBJ_DIR)/%.o: $(SRC_DIR)/%.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -DTEST -c $< -o $@

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

-include $(DEPS)
