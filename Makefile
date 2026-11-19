CC       = gcc
PREFIX  ?= /usr/local
BINDIR   = $(PREFIX)/bin
DATADIR  = $(PREFIX)/share/blurred_files
APPDIR   = $(PREFIX)/share/applications
ICONDIR  = $(PREFIX)/share/icons/hicolor/scalable/apps

CFLAGS   = -Wall -O2 -DDATADIR=\"$(DATADIR)\" $(shell pkg-config --cflags gtk4)
LDFLAGS  = $(shell pkg-config --libs gtk4) -lm

SRC_DIR  = src
BUILD_DIR= build
TARGET   = $(BUILD_DIR)/blurred_files

SRCS     = $(wildcard $(SRC_DIR)/*.c)
OBJS     = $(patsubst $(SRC_DIR)/%.c,$(BUILD_DIR)/%.o,$(SRCS))

all: $(TARGET)

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

$(TARGET): $(OBJS)
	$(CC) $(OBJS) -o $(TARGET) $(LDFLAGS)
	strip --strip-unneeded $(TARGET)

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c $(SRC_DIR)/app.h | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

install: $(TARGET)
	install -Dm755 $(TARGET) $(DESTDIR)$(BINDIR)/blurred_files
	install -Dm644 $(SRC_DIR)/style.css $(DESTDIR)$(DATADIR)/style.css
	install -Dm644 $(SRC_DIR)/blurred_files.svg $(DESTDIR)$(ICONDIR)/blurred_files.svg
	install -Dm644 blurred_files.desktop $(DESTDIR)$(APPDIR)/blurred_files.desktop
	gtk-update-icon-cache -f -t $(DESTDIR)$(PREFIX)/share/icons/hicolor 2>/dev/null || true
	update-desktop-database $(DESTDIR)$(APPDIR) 2>/dev/null || true

uninstall:
	rm -f $(DESTDIR)$(BINDIR)/blurred_files
	rm -rf $(DESTDIR)$(DATADIR)
	rm -f $(DESTDIR)$(ICONDIR)/blurred_files.svg
	rm -f $(DESTDIR)$(APPDIR)/blurred_files.desktop
	gtk-update-icon-cache -f -t $(DESTDIR)$(PREFIX)/share/icons/hicolor 2>/dev/null || true
	update-desktop-database $(DESTDIR)$(APPDIR) 2>/dev/null || true

run: $(TARGET)
	GTK_A11Y=none ./$(TARGET)

clean:
	rm -rf $(BUILD_DIR)

.PHONY: all install uninstall run clean

TEST_OBJS := $(filter-out build/main.o, $(OBJS))
ASAN_FLAGS := -fsanitize=address,undefined -fno-omit-frame-pointer -g

test: build/test_runner
	@if command -v xvfb-run >/dev/null 2>&1; then \
		xvfb-run -a -s "-screen 0 1280x800x24" ./build/test_runner --verbose; \
	else \
		./build/test_runner --verbose; \
	fi

test: build/test_runner
	GTK_A11Y=none LSAN_OPTIONS=suppressions=tests/lsan.supp xvfb-run -a -s "-screen 0 1280x800x24" ./build/test_runner --verbose

build/test_runner: tests/test_main.c $(TEST_OBJS)
	$(CC) $(CFLAGS) $(ASAN_FLAGS) $^ -o $@ $(LDFLAGS)