cpu_count := $(shell nproc 2>/dev/null || sysctl -n hw.logicalcpu 2>/dev/null || echo 4)

.PHONY: all
all: format jsrt

.PHONY: format
format: clang-format prettier

.PHONY: jsrt
jsrt:
	mkdir -p target/release
	cd target/release && cmake -G "Unix Makefiles" -DCMAKE_BUILD_TYPE=Release $(CURDIR) && make -j$(cpu_count)
	ls -alh target/release/jsrt
	@cd deps/wamr && git restore core/version.h 2>/dev/null || true

.PHONY: jsrt_g
jsrt_g:
	mkdir -p target/debug
	cd target/debug && cmake -G "Unix Makefiles" -DCMAKE_BUILD_TYPE=Debug $(CURDIR) && make -j$(cpu_count)
	ls -alh target/debug/jsrt
	@cd deps/wamr && git restore core/version.h 2>/dev/null || true

.PHONY: jsrt_m
jsrt_m:
	mkdir -p target/asan
	cd target/asan && cmake -G "Unix Makefiles" -DENABLE_ASAN=ON -DCMAKE_BUILD_TYPE=Debug $(CURDIR) && make -j$(cpu_count)
	ls -alh target/asan/jsrt
	@cd deps/wamr && git restore core/version.h 2>/dev/null || true

.PHONY: jsrt_cov
jsrt_cov:
	mkdir -p target/coverage
	cd target/coverage && cmake -G "Unix Makefiles" -DENABLE_COVERAGE=ON -DCMAKE_BUILD_TYPE=Release $(CURDIR) && make -j$(cpu_count)
	ls -alh target/coverage/jsrt
	@cd deps/wamr && git restore core/version.h 2>/dev/null || true

.PHONY: jsrt_static
jsrt_static:
	mkdir -p target/static
	@echo "Building static jsrt with all dependencies statically linked..."
	@if [ "$(OS)" = "Windows_NT" ] || [ -n "$(MSYSTEM)" ]; then \
		echo "Windows detected: Applying pre-build fixes..."; \
		chmod +x scripts/pre-build-windows.sh scripts/patch-libuv-windows.sh scripts/cleanup-submodules.sh; \
		./scripts/pre-build-windows.sh; \
		echo "Using MSYS Makefiles generator"; \
		cd target/static && cmake -G "MSYS Makefiles" -DCMAKE_BUILD_TYPE=Release -DBUILD_SHARED_LIBS=OFF -DJSRT_STATIC_OPENSSL=ON -DCMAKE_MAKE_PROGRAM=/usr/bin/make $(CURDIR); \
		if [ $$? -eq 0 ]; then \
			echo "CMake configuration successful, building..."; \
			/usr/bin/make -j$(cpu_count); \
			BUILD_RESULT=$$?; \
		else \
			echo "CMake configuration failed"; \
			BUILD_RESULT=1; \
		fi; \
		cd ../..; \
		if [ -f scripts/post-build-windows.sh ]; then ./scripts/post-build-windows.sh; fi; \
		./scripts/cleanup-submodules.sh; \
		exit $$BUILD_RESULT; \
	else \
		echo "Unix-like system: Using Unix Makefiles generator"; \
		cd target/static && cmake -G "Unix Makefiles" -DCMAKE_BUILD_TYPE=Release -DBUILD_SHARED_LIBS=OFF -DJSRT_STATIC_OPENSSL=ON $(CURDIR) && make -j$(cpu_count); \
	fi
	ls -alh target/static/jsrt 2>/dev/null || dir target\\static\\jsrt.exe
	@echo "Cleaning up submodule modifications..."
	@cd deps/wamr && git restore core/version.h 2>/dev/null || true
	@cd deps/libuv && git checkout -- . 2>/dev/null || true
	@cd deps/quickjs && git checkout -- . 2>/dev/null || true
	@cd deps/llhttp && git checkout -- . 2>/dev/null || true
	@echo "All submodules reset to clean state"

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

.PHONY: test_static
test_static: jsrt_static npm-install
	cd target/static && ctest --verbose

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
coverage-merged: test_cov wpt-download
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
	python3 scripts/run-wpt.py --jsrt $(CURDIR)/target/release/jsrt --wpt-dir $(CURDIR)/wpt $(if $(N),--category $(N)) $(if $(SHOW_ALL_FAILURES),--show-all-failures) $(if $(MAX_FAILURES),--max-failures $(MAX_FAILURES))

.PHONY: wpt_g
wpt_g: jsrt_g wpt-download
	@echo "Running Web Platform Tests with debug build..."
	python3 scripts/run-wpt.py --jsrt $(CURDIR)/target/debug/jsrt --wpt-dir $(CURDIR)/wpt --verbose $(if $(N),--category $(N)) $(if $(SHOW_ALL_FAILURES),--show-all-failures) $(if $(MAX_FAILURES),--max-failures $(MAX_FAILURES))

.PHONY: wpt_cov
wpt_cov: jsrt_cov wpt-download
	@echo "Running Web Platform Tests with coverage..."
	python3 scripts/run-wpt.py --jsrt $(CURDIR)/target/coverage/jsrt --wpt-dir $(CURDIR)/wpt || echo "Some WPT tests failed, but continuing..."

.PHONY: wpt_static
wpt_static: jsrt_static wpt-download
	@echo "Running Web Platform Tests with static build..."
	python3 scripts/run-wpt.py --jsrt $(CURDIR)/target/static/jsrt --wpt-dir $(CURDIR)/wpt $(if $(N),--category $(N)) $(if $(SHOW_ALL_FAILURES),--show-all-failures) $(if $(MAX_FAILURES),--max-failures $(MAX_FAILURES))

.PHONY: wpt-list
wpt-list:
	@echo "Available WPT test categories:"
	python3 scripts/run-wpt.py --list-categories

# Docker and Claude Code commands
DOCKER_IMAGE_NAME = jsrt-claude-dev
DOCKER_TAG = latest

.PHONY: docker-build
docker-build:
	@echo "Building Docker image for Claude Code development environment..."
	docker build -t $(DOCKER_IMAGE_NAME):$(DOCKER_TAG) -f Dockerfile.claude .
	@echo "✓ Docker image built: $(DOCKER_IMAGE_NAME):$(DOCKER_TAG)"

.PHONY: claude
claude: docker-build
	@echo "Starting Claude Code in Docker environment..."
	@echo "Repository mapped to /repo inside container"
	@echo "Running as current user (UID=$(shell id -u), GID=$(shell id -g))"
	@echo "Git configured as: $(shell git config user.name) <$(shell git config user.email)>"
	@echo "Claude Code will run with unsafe operations allowed"
	docker run -it --rm \
		--user "$(shell id -u):$(shell id -g)" \
		-v "$(CURDIR):/repo" \
		-v "/etc/passwd:/etc/passwd:ro" \
		-v "/etc/group:/etc/group:ro" \
		-w /repo \
		--name claude-session-$(shell basename $(CURDIR)) \
		-e HOME="/tmp" \
		-e USER="$(shell whoami)" \
		-e GIT_USER_NAME="$(shell git config user.name)" \
		-e GIT_USER_EMAIL="$(shell git config user.email)" \
		-e ANTHROPIC_BASE_URL=$(ANTHROPIC_BASE_URL) \
		-e ANTHROPIC_AUTH_TOKEN=$(ANTHROPIC_AUTH_TOKEN) \
		$(DOCKER_IMAGE_NAME):$(DOCKER_TAG)

.PHONY: claude-shell
claude-shell: docker-build
	@echo "Starting interactive shell in Claude development environment..."
	@echo "Running as current user (UID=$(shell id -u), GID=$(shell id -g))"
	@echo "Git configured as: $(shell git config user.name) <$(shell git config user.email)>"
	docker run -it --rm \
		--user "$(shell id -u):$(shell id -g)" \
		-v "$(CURDIR):/repo" \
		-v "/etc/passwd:/etc/passwd:ro" \
		-v "/etc/group:/etc/group:ro" \
		-w /repo \
		--name claude-shell-$(shell basename $(CURDIR)) \
		-e HOME="/tmp" \
		-e USER="$(shell whoami)" \
		-e GIT_USER_NAME="$(shell git config user.name)" \
		-e GIT_USER_EMAIL="$(shell git config user.email)" \
		--entrypoint /bin/bash \
		$(DOCKER_IMAGE_NAME):$(DOCKER_TAG)

.PHONY: claude-shell-attach
claude-shell-attach:
	@echo "Attaching to existing Claude shell session..."
	docker exec -it claude-shell-$(shell basename $(CURDIR)) /bin/bash

.PHONY: docker-clean
docker-clean:
	@echo "Cleaning up Docker images and containers..."
	docker rmi $(DOCKER_IMAGE_NAME):$(DOCKER_TAG) 2>/dev/null || true
	@echo "✓ Docker cleanup completed"

.PHONY: help
help:
	@echo "JSRT Makefile Commands"
	@echo "====================="
	@echo ""
	@echo "Build Commands:"
	@echo "  make, make jsrt    - Build release version"
	@echo "  make jsrt_g        - Build debug version"
	@echo "  make jsrt_m        - Build with AddressSanitizer"
	@echo "  make jsrt_cov      - Build with coverage"
	@echo "  make jsrt_static   - Build with static linking (includes OpenSSL)"
	@echo "  make clean         - Clean build artifacts"
	@echo ""
	@echo "Code Quality:"
	@echo "  make clang-format  - Format C/C++ code"
	@echo "  make prettier      - Format JavaScript code"
	@echo ""
	@echo "Testing:"
	@echo "  make test          - Run all tests (release build)"
	@echo "  make test_static   - Run all tests (static build)"
	@echo "  make test-quick    - Run essential tests only"
	@echo "  make test-fast     - Run tests in parallel"
	@echo "  make wpt           - Run Web Platform Tests (release build)"
	@echo "  make wpt_static    - Run Web Platform Tests (static build)"
	@echo "  make coverage      - Generate coverage report"
	@echo ""
	@echo "WPT Options (use with make wpt/wpt_g/wpt_static):"
	@echo "  N=category         - Run specific test category"
	@echo "  SHOW_ALL_FAILURES=1 - Show all test failures (no limit)"
	@echo "  MAX_FAILURES=N     - Show max N failures per test (default: 10)"
	@echo ""
	@echo "Docker & Dev Environment:"
	@echo "  make docker-build  - Build Claude Code Docker image"
	@echo "  make claude        - Run Claude Code in Docker"
	@echo "  make claude-shell  - Interactive shell in Docker"
	@echo "  make docker-clean  - Clean Docker resources"
	@echo ""
	@echo "See docs/dev-container-setup.md for development environment setup"
