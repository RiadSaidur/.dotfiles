CC ?= gcc
CFLAGS ?= -O2 -Wall -Wextra -Wno-unused-parameter
# Winner bake-off also validated -Os hotpath; keep -O2 for GTK stability.

PKGS := gtk+-3.0 gtk-layer-shell-0
CFLAGS += $(shell pkg-config --cflags $(PKGS))
LDFLAGS += $(shell pkg-config --libs $(PKGS)) -lm

BIN := shadow-cat
SRC := shadow_cat.c
TEST_BIN := tests/test_unit

.PHONY: all clean test bench hotpath

all: $(BIN)

$(BIN): $(SRC)
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS)

$(TEST_BIN): tests/test_unit.c
	$(CC) -O2 -Wall -Wextra -o $@ $< -lm

test: $(TEST_BIN)
	./$(TEST_BIN)

bench: $(BIN)
	bash tests/bench_runtime.sh

hotpath:
	$(CC) -O2 -o tests/bench_hotpath tests/bench_hotpath.c -lm
	./tests/bench_hotpath | tee tests/hotpath_baseline.txt
	$(CC) -O2 -DBENCH_SKIP_REDUNDANT_MARGINS -o tests/bench_hotpath_m tests/bench_hotpath.c -lm
	./tests/bench_hotpath_m | tee tests/hotpath_margins.txt
	$(CC) -O0 -o tests/bench_hotpath_O0 tests/bench_hotpath.c -lm
	./tests/bench_hotpath_O0 | tee tests/hotpath_O0.txt
	$(CC) -Os -o tests/bench_hotpath_Os tests/bench_hotpath.c -lm
	./tests/bench_hotpath_Os | tee tests/hotpath_Os.txt
	$(CC) -O3 -o tests/bench_hotpath_O3 tests/bench_hotpath.c -lm
	./tests/bench_hotpath_O3 | tee tests/hotpath_O3.txt

clean:
	rm -f $(BIN) $(TEST_BIN) tests/bench_hotpath tests/bench_hotpath_m \
		tests/bench_hotpath_O0 tests/bench_hotpath_Os tests/bench_hotpath_O3
