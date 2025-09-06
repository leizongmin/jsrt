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

# clang-format version standardization
# Ensures all environments use consistent clang-format version for code formatting
# Downloads and installs clang-format v20 to target/tools/ if not available
CLANG_FORMAT_VERSION = 20
CLANG_FORMAT_DIR = target/tools
CLANG_FORMAT_BIN = $(CLANG_FORMAT_DIR)/clang-format-$(CLANG_FORMAT_VERSION)

# Check and install clang-format v20 if needed
.PHONY: ensure-clang-format
ensure-clang-format:
	@if [ ! -f "$(CLANG_FORMAT_BIN)" ] || ! $(CLANG_FORMAT_BIN) --version 2>/dev/null | grep -q "version $(CLANG_FORMAT_VERSION)"; then \
		echo "Setting up clang-format v$(CLANG_FORMAT_VERSION)..."; \
		mkdir -p $(CLANG_FORMAT_DIR); \
		cd $(CLANG_FORMAT_DIR) && \
		if command -v clang-format-$(CLANG_FORMAT_VERSION) >/dev/null 2>&1; then \
			echo "Using system clang-format-$(CLANG_FORMAT_VERSION)"; \
			ln -sf $$(which clang-format-$(CLANG_FORMAT_VERSION)) clang-format-$(CLANG_FORMAT_VERSION); \
		elif command -v apt-get >/dev/null 2>&1; then \
			echo "Installing clang-format-$(CLANG_FORMAT_VERSION) via apt..."; \
			if ! apt-cache search clang-format-$(CLANG_FORMAT_VERSION) | grep -q clang-format-$(CLANG_FORMAT_VERSION); then \
				echo "Adding LLVM repository..."; \
				wget -qO- https://apt.llvm.org/llvm-snapshot.gpg.key | sudo tee /etc/apt/trusted.gpg.d/apt.llvm.org.asc >/dev/null; \
				echo "deb http://apt.llvm.org/jammy/ llvm-toolchain-jammy-$(CLANG_FORMAT_VERSION) main" | sudo tee /etc/apt/sources.list.d/llvm-toolchain-jammy-$(CLANG_FORMAT_VERSION).list >/dev/null; \
				sudo apt-get update >/dev/null 2>&1; \
			fi; \
			sudo apt-get install -y clang-format-$(CLANG_FORMAT_VERSION) >/dev/null 2>&1 && \
			ln -sf $$(which clang-format-$(CLANG_FORMAT_VERSION)) clang-format-$(CLANG_FORMAT_VERSION); \
		elif command -v snap >/dev/null 2>&1; then \
			echo "Attempting to install via snap..."; \
			sudo snap install clang-format-$(CLANG_FORMAT_VERSION) --devmode 2>/dev/null || \
			sudo snap install clang-format --devmode 2>/dev/null; \
			ln -sf $$(which clang-format) clang-format-$(CLANG_FORMAT_VERSION) 2>/dev/null; \
		else \
			echo "Package manager not found. Creating symlink to system clang-format..."; \
			which clang-format >/dev/null 2>&1 && \
			ln -sf $$(which clang-format) clang-format-$(CLANG_FORMAT_VERSION) || \
			( echo "Error: clang-format not found. Please install clang-format manually."; exit 1 ); \
		fi; \
		if [ -f clang-format-$(CLANG_FORMAT_VERSION) ] || [ -L clang-format-$(CLANG_FORMAT_VERSION) ]; then \
			chmod +x clang-format-$(CLANG_FORMAT_VERSION) 2>/dev/null || true; \
			version_output=$$(./clang-format-$(CLANG_FORMAT_VERSION) --version 2>/dev/null || echo "unknown"); \
			if echo "$$version_output" | grep -q "version $(CLANG_FORMAT_VERSION)"; then \
				echo "✓ clang-format v$(CLANG_FORMAT_VERSION) setup completed"; \
			else \
				echo "⚠ Warning: Using clang-format ($$version_output) - not exactly v$(CLANG_FORMAT_VERSION)"; \
			fi; \
		else \
			echo "Warning: Failed to setup clang-format v$(CLANG_FORMAT_VERSION), falling back to system version"; \
			which clang-format >/dev/null 2>&1 && \
			ln -sf $$(which clang-format) clang-format-$(CLANG_FORMAT_VERSION) || \
			( echo "Error: No clang-format available. Please install clang-format manually."; exit 1 ); \
		fi; \
	else \
		version_output=$$($(CLANG_FORMAT_BIN) --version 2>/dev/null || echo "unknown"); \
		if echo "$$version_output" | grep -q "version $(CLANG_FORMAT_VERSION)"; then \
			echo "✓ clang-format v$(CLANG_FORMAT_VERSION) already available"; \
		else \
			echo "✓ clang-format available ($$version_output)"; \
		fi; \
	fi

.PHONY: clang-format
clang-format: ensure-clang-format
	@echo "Formatting C/C++ code with clang-format v$(CLANG_FORMAT_VERSION)..."
	@$(CLANG_FORMAT_BIN) --version | head -1
	@find src -type f -name "*.c" -o -name "*.h" | xargs -r $(CLANG_FORMAT_BIN) -i
	@find test -type f -name "*.c" -o -name "*.h" | xargs -r $(CLANG_FORMAT_BIN) -i
	@echo "✓ Code formatting completed"

.PHONY: npm-install
npm-install:
	@if [ ! -d "node_modules" ] || [ ! -f "node_modules/.package-lock.json" ]; then \
		echo "Installing npm dependencies..."; \
		npm install; \
	else \
		echo "✓ npm dependencies already installed"; \
	fi

.PHONY: prettier
prettier: npm-install
	npx prettier --write 'examples/**/*.{js,mjs}' 'test/**/*.{js,mjs}'

.PHONY: test
test: jsrt npm-install
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

.PHONY: coverage-merged
coverage-merged: test_cov
	@echo "Generating merged coverage report (regular tests + WPT)..."
	# Capture coverage from regular tests (already executed by test_cov dependency)
	lcov --capture --directory target/coverage --output-file target/coverage/coverage_tests.info
	# Reset coverage counters for WPT
	lcov --zerocounters --directory target/coverage
	# Run WPT tests with coverage (ignore test failures - non-zero exit codes are expected)
	python3 scripts/run-wpt.py --jsrt $(CURDIR)/target/coverage/jsrt --wpt-dir $(CURDIR)/wpt || echo "Some WPT tests failed, but continuing with coverage generation..."
	# Capture coverage from WPT tests
	lcov --capture --directory target/coverage --output-file target/coverage/coverage_wpt.info
	# Merge both coverage files
	lcov --add-tracefile target/coverage/coverage_tests.info --add-tracefile target/coverage/coverage_wpt.info --output-file target/coverage/coverage_merged.info
	# Filter out unwanted files from merged coverage
	lcov --remove target/coverage/coverage_merged.info 'deps/*' 'target/*' 'test/*' --output-file target/coverage/coverage_filtered.info --ignore-errors unused
	# Generate HTML report
	genhtml target/coverage/coverage_filtered.info --output-directory target/coverage/html
	@echo "Merged coverage report generated in target/coverage/html/"
	@echo "Overall merged coverage:"
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

.PHONY: wpt_cov
wpt_cov: jsrt_cov wpt-download
	@echo "Running Web Platform Tests with coverage..."
	python3 scripts/run-wpt.py --jsrt $(CURDIR)/target/coverage/jsrt --wpt-dir $(CURDIR)/wpt || echo "Some WPT tests failed, but continuing..."

.PHONY: wpt-list
wpt-list:
	@echo "Available WPT test categories:"
	python3 scripts/run-wpt.py --list-categories
