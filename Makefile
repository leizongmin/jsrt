cpu_count := $(shell nproc 2>/dev/null || sysctl -n hw.logicalcpu 2>/dev/null || echo 4)

.PHONY: all
all: clang-format prettier jsrt

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
	cd target/coverage && cmake -G "Unix Makefiles" -DENABLE_COVERAGE=ON -DCMAKE_BUILD_TYPE=Release $(CURDIR) && make -j$(cpu_count)
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

.PHONY: prettier
prettier:
	npx prettier --write 'examples/**/*.{js,mjs}' 'test/**/*.{js,mjs}'

.PHONY: test
test: jsrt
	cd target/release && ctest --verbose

# Fast parallel test execution (4 jobs)
.PHONY: test-fast
test-fast: jsrt
	cd target/release && ctest --verbose --parallel 4

# Quick test - only essential tests without network dependencies  
.PHONY: test-quick
test-quick: jsrt
	cd target/release && ctest --verbose -R "test_(assert|base64|console|timer|http_llhttp_fast)"

.PHONY: test_g
test_g: jsrt_g
	cd target/debug && ctest --verbose

.PHONY: test_m
test_m: jsrt_m
	cd target/asan && ctest --verbose

.PHONY: test_cov
test_cov: jsrt_cov
	cd target/coverage && ctest --verbose

.PHONY: coverage
coverage: test_cov
	@echo "Generating coverage report..."
	lcov --capture --directory target/coverage --output-file target/coverage/coverage.info
	lcov --remove target/coverage/coverage.info 'deps/*' 'target/*' 'test/*' --output-file target/coverage/coverage_filtered.info --ignore-errors unused
	genhtml target/coverage/coverage_filtered.info --output-directory target/coverage/html
	@echo "Coverage report generated in target/coverage/html/"
	@echo "Overall coverage:"
	lcov --summary target/coverage/coverage_filtered.info

.PHONY: wpt-download
wpt-download:
	@echo "Downloading Web Platform Tests..."
	@if [ ! -d "wpt" ]; then \
		echo "Creating wpt directory..."; \
		mkdir -p wpt; \
		echo "Downloading WPT master branch..."; \
		curl -L https://github.com/web-platform-tests/wpt/archive/refs/heads/master.zip -o wpt-master.zip; \
		echo "Extracting WPT..."; \
		unzip -q wpt-master.zip; \
		mv wpt-master/* wpt/; \
		mv wpt-master/.* wpt/ 2>/dev/null || true; \
		rm -rf wpt-master; \
		rm wpt-master.zip; \
		echo "WPT downloaded successfully"; \
	else \
		echo "WPT directory already exists. Use 'make wpt-update' to refresh."; \
	fi

.PHONY: wpt-update
wpt-update:
	@echo "Updating Web Platform Tests..."
	@echo "Removing existing WPT directory..."
	@rm -rf wpt
	@$(MAKE) wpt-download

.PHONY: wpt
wpt: jsrt wpt-download
	@echo "Running Web Platform Tests for WinterCG Minimum Common API..."
	python3 scripts/run-wpt.py --jsrt $(CURDIR)/target/release/jsrt --wpt-dir $(CURDIR)/wpt

.PHONY: wpt_g
wpt_g: jsrt_g wpt-download
	@echo "Running Web Platform Tests with debug build..."
	python3 scripts/run-wpt.py --jsrt $(CURDIR)/target/debug/jsrt --wpt-dir $(CURDIR)/wpt --verbose

.PHONY: wpt-list
wpt-list:
	@echo "Available WPT test categories:"
	python3 scripts/run-wpt.py --list-categories
