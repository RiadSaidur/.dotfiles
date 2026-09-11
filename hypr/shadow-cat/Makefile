# shadow-cat — Hyprland desktop pet
VERSION  ?= 0.1.0
CC       ?= gcc
CFLAGS   ?= -O2 -Wall -Wextra -Wno-unused-parameter
CFLAGS   += -DSHADOW_CAT_VERSION=\"$(VERSION)\"

PKGS     := gtk+-3.0 gtk-layer-shell-0
CFLAGS   += $(shell pkg-config --cflags $(PKGS))
LDFLAGS  += $(shell pkg-config --libs $(PKGS)) -lm

PREFIX   ?= /usr
BINDIR   ?= $(PREFIX)/bin
DOCDIR   ?= $(PREFIX)/share/doc/shadow-cat
LICENSEDIR ?= $(PREFIX)/share/licenses/shadow-cat
DESTDIR  ?=

BIN      := shadow-cat
SRC      := shadow_cat.c
TEST_BIN := tests/test_unit

.PHONY: all clean test install uninstall dist check-deps

all: check-deps $(BIN)

check-deps:
	@pkg-config --exists $(PKGS) || \
	  (echo "missing deps — need: gtk3 gtk-layer-shell (pkg: $(PKGS))" >&2; exit 1)

$(BIN): $(SRC)
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS)

$(TEST_BIN): tests/test_unit.c
	$(CC) -O2 -Wall -Wextra -o $@ $< -lm

test: $(TEST_BIN)
	./$(TEST_BIN)

install: $(BIN)
	install -Dm755 $(BIN) "$(DESTDIR)$(BINDIR)/$(BIN)"
	install -Dm644 LICENSE "$(DESTDIR)$(LICENSEDIR)/LICENSE"
	install -Dm644 README.md "$(DESTDIR)$(DOCDIR)/README.md"
	install -Dm644 packaging/hyprland.conf.example \
	  "$(DESTDIR)$(DOCDIR)/hyprland.conf.example"

uninstall:
	rm -f "$(DESTDIR)$(BINDIR)/$(BIN)"
	rm -f "$(DESTDIR)$(LICENSEDIR)/LICENSE"
	rm -f "$(DESTDIR)$(DOCDIR)/README.md"
	rm -f "$(DESTDIR)$(DOCDIR)/hyprland.conf.example"
	rmdir "$(DESTDIR)$(LICENSEDIR)" 2>/dev/null || true
	rmdir "$(DESTDIR)$(DOCDIR)" 2>/dev/null || true

# Source tarball for release PKGBUILD (not the -git package).
dist:
	@mkdir -p dist
	tar --transform 's,^,shadow-cat-$(VERSION)/,' -czf \
	  dist/shadow-cat-$(VERSION).tar.gz \
	  LICENSE README.md Makefile shadow_cat.c \
	  packaging/hyprland.conf.example packaging/AUR.md
	@echo "wrote dist/shadow-cat-$(VERSION).tar.gz"

clean:
	rm -f $(BIN) $(TEST_BIN) \
	  tests/bench_hotpath tests/bench_hotpath_m \
	  tests/bench_hotpath_O0 tests/bench_hotpath_Os tests/bench_hotpath_O3
	rm -rf dist src pkg *.pkg.tar* *.pkg.tar.zst
