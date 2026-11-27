CC       = gcc
PREFIX  ?= /usr/local
BINDIR   = $(PREFIX)/bin
DATADIR  = $(PREFIX)/share/blurred_files
APPDIR   = $(PREFIX)/share/applications
ICONDIR  = $(PREFIX)/share/icons/hicolor/scalable/apps

PKGS     = gtk4 gtk4-layer-shell-0
CFLAGS   = -Wall -O2 -DDATADIR=\"$(DATADIR)\" $(shell pkg-config --cflags $(PKGS))
LDFLAGS  = $(shell pkg-config --libs $(PKGS)) -lm
STRIP   ?= strip

SRC_DIR  = src
BUILD_DIR= build
TARGET   = $(BUILD_DIR)/blurred_files

SRCS     = $(wildcard $(SRC_DIR)/*.c)
OBJS     = $(patsubst $(SRC_DIR)/%.c,$(BUILD_DIR)/%.o,$(SRCS))

TEST_OBJS := $(filter-out $(BUILD_DIR)/main.o, $(OBJS))
ASAN_FLAGS := -fsanitize=address,undefined -fno-omit-frame-pointer -g

all: $(TARGET)

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

$(TARGET): $(OBJS) | $(BUILD_DIR)
	$(CC) $(CFLAGS) $(OBJS) -o $(TARGET) $(LDFLAGS)
	$(STRIP) --strip-unneeded $(TARGET)

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c $(SRC_DIR)/app.h | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

sanitize: CFLAGS = -Wall -O1 -g -DDATADIR=\"$(DATADIR)\" $(shell pkg-config --cflags $(PKGS)) $(ASAN_FLAGS)
sanitize: LDFLAGS += $(ASAN_FLAGS)
sanitize: STRIP = true
sanitize: clean $(TARGET) $(BUILD_DIR)/test_runner

$(BUILD_DIR)/test_runner: tests/test_main.c $(TEST_OBJS) | $(BUILD_DIR)
	$(CC) -Wl,--no-as-needed -lasan -Wl,--as-needed $(ASAN_FLAGS) $(CFLAGS) $^ -o $@ $(LDFLAGS)

test: $(BUILD_DIR)/test_runner
	@if command -v xvfb-run >/dev/null 2>&1; then \
		GTK_A11Y=none LSAN_OPTIONS=suppressions=tests/lsan.supp xvfb-run -a -s "-screen 0 1280x800x24" ./$(BUILD_DIR)/test_runner --verbose; \
	else \
		GTK_A11Y=none LSAN_OPTIONS=suppressions=tests/lsan.supp ./$(BUILD_DIR)/test_runner --verbose; \
	fi

install: $(TARGET)
	install -Dm755 $(TARGET) $(DESTDIR)$(BINDIR)/blurred_files
	install -Dm644 src/bfiles.css $(DESTDIR)$(DATADIR)/bfiles.css
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

.PHONY: all sanitize test install uninstall run clean