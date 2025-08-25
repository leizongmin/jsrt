cpu_count := $(shell nproc 2>/dev/null || sysctl -n hw.logicalcpu 2>/dev/null || echo 4)

.PHONY: all
all: clang-format jsrt

.PHONY: jsrt
jsrt:
	mkdir -p target/release
	cd target/release && cmake -G "Unix Makefiles" -DCMAKE_BUILD_TYPE=Release $(CURDIR) && make -j$(cpu_count)
	ls -alh target/release/jsrt

.PHONY: jsrt_g
jsrt_g:
	mkdir -p target/debug
	cd target/debug && cmake -G "Unix Makefiles" -DCMAKE_BUILD_TYPE=Debug $(CURDIR) && make -j$(cpu_count)
	ls -alh target/debug/jsrt

.PHONY: jsrt_m
jsrt_m:
	mkdir -p target/asan
	cd target/asan && cmake -G "Unix Makefiles" -DENABLE_ASAN=ON -DCMAKE_BUILD_TYPE=Debug $(CURDIR) && make -j$(cpu_count)
	ls -alh target/asan/jsrt

.PHONY: jsrt_cov
jsrt_cov:
	mkdir -p target/coverage
	cd target/coverage && cmake -G "Unix Makefiles" -DENABLE_COVERAGE=ON -DCMAKE_BUILD_TYPE=Debug $(CURDIR) && make -j$(cpu_count)
	ls -alh target/coverage/jsrt

.PHONY: jsrt_s
jsrt_s: jsrt
	cp -f target/release/jsrt target/release/jsrt_s
	strip target/release/jsrt_s
	upx --best target/release/jsrt_s
	ls -alh target/release/jsrt_s

.PHONY: clean
clean:
	rm -rf target core.*

.PHONY: clang-format
clang-format:
	find src -type f -name "*.c" -o -name "*.h" | xargs clang-format -i
	find test -type f -name "*.c" -o -name "*.h" | xargs clang-format -i

.PHONY: test
test: jsrt
	make test -C target/release

.PHONY: test_g
test_g: jsrt_g
	make test -C target/debug

.PHONY: test_m
test_m: jsrt_m
	make test -C target/asan

.PHONY: test_cov
test_cov: jsrt_cov
	make test -C target/coverage

.PHONY: coverage
coverage: test_cov
	@echo "Generating coverage report..."
	lcov --capture --directory target/coverage --output-file target/coverage/coverage.info
	lcov --remove target/coverage/coverage.info 'deps/*' 'target/*' 'test/*' --output-file target/coverage/coverage_filtered.info --ignore-errors unused
	genhtml target/coverage/coverage_filtered.info --output-directory target/coverage/html
	@echo "Coverage report generated in target/coverage/html/"
	@echo "Overall coverage:"
	lcov --summary target/coverage/coverage_filtered.info
