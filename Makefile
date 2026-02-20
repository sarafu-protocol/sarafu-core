# Sarafu Blockchain Makefile
# Comprehensive build, test, and deployment commands

.PHONY: help build clean test docker-build docker-up docker-down docker-restart docker-logs docker-status docker-clean run-local run-validator keys genesis format lint check coverage install deps

# Default target
.DEFAULT_GOAL := help

# Colors for output
BLUE := \033[0;34m
GREEN := \033[0;32m
YELLOW := \033[1;33m
RED := \033[0;31m
NC := \033[0m # No Color

# Build configuration
BUILD_DIR := build
BUILD_TYPE ?= Release
CMAKE_FLAGS := -DCMAKE_BUILD_TYPE=$(BUILD_TYPE)
NPROC := $(shell nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 4)

# Docker configuration
DOCKER_COMPOSE := docker-compose -f docker/docker-compose.yml
VALIDATOR_COUNT := 10

##@ Help

help: ## Display this help message
	@echo "$(BLUE)Sarafu Blockchain - Available Commands$(NC)"
	@echo ""
	@awk 'BEGIN {FS = ":.*##"; printf "Usage:\n  make $(GREEN)<target>$(NC)\n"} /^[a-zA-Z_0-9-]+:.*?##/ { printf "  $(GREEN)%-20s$(NC) %s\n", $$1, $$2 } /^##@/ { printf "\n$(BLUE)%s$(NC)\n", substr($$0, 5) } ' $(MAKEFILE_LIST)

##@ Build

deps: ## Install system dependencies (Ubuntu/Debian)
	@echo "$(YELLOW)Installing system dependencies...$(NC)"
	sudo apt-get update
	sudo apt-get install -y \
		build-essential cmake git pkg-config \
		libssl-dev zlib1g-dev libsnappy-dev \
		libbz2-dev liblz4-dev libzstd-dev libsodium-dev \
		protobuf-compiler libprotobuf-dev \
		libgrpc++-dev protobuf-compiler-grpc \
		librocksdb-dev curl jq
	@echo "$(GREEN)Dependencies installed successfully$(NC)"

build: ## Build the project (default: Release)
	@echo "$(YELLOW)Building Sarafu node ($(BUILD_TYPE))...$(NC)"
	@mkdir -p $(BUILD_DIR)
	@cd $(BUILD_DIR) && cmake $(CMAKE_FLAGS) .. && cmake --build . -j$(NPROC)
	@echo "$(GREEN)Build complete: $(BUILD_DIR)/sarafu-node$(NC)"

build-debug: ## Build with debug symbols
	@$(MAKE) build BUILD_TYPE=Debug

build-release: ## Build optimized release version
	@$(MAKE) build BUILD_TYPE=Release

rebuild: clean build ## Clean and rebuild from scratch

install: build ## Install the binary to /usr/local/bin
	@echo "$(YELLOW)Installing sarafu-node...$(NC)"
	sudo install -m 755 $(BUILD_DIR)/sarafu-node /usr/local/bin/
	@echo "$(GREEN)Installed to /usr/local/bin/sarafu-node$(NC)"

clean: ## Clean build artifacts
	@echo "$(YELLOW)Cleaning build directory...$(NC)"
	@rm -rf $(BUILD_DIR)
	@echo "$(GREEN)Clean complete$(NC)"

clean-all: clean docker-clean ## Clean everything including Docker

##@ Testing

test: build ## Run all tests
	@echo "$(YELLOW)Running tests...$(NC)"
	@cd $(BUILD_DIR) && ctest --output-on-failure -j$(NPROC)

test-verbose: build ## Run tests with verbose output
	@cd $(BUILD_DIR) && ctest --verbose -j$(NPROC)

test-unit: build ## Run unit tests only
	@cd $(BUILD_DIR) && ctest -R "unit_" --output-on-failure

test-integration: build ## Run integration tests only
	@cd $(BUILD_DIR) && ctest -R "integration_" --output-on-failure

test-consensus: build ## Run consensus tests
	@cd $(BUILD_DIR) && ctest -R "consensus" --output-on-failure

coverage: ## Generate code coverage report
	@echo "$(YELLOW)Generating coverage report...$(NC)"
	@mkdir -p $(BUILD_DIR)
	@cd $(BUILD_DIR) && cmake -DCMAKE_BUILD_TYPE=Debug -DENABLE_COVERAGE=ON .. && \
		cmake --build . -j$(NPROC) && \
		ctest --output-on-failure && \
		lcov --capture --directory . --output-file coverage.info && \
		lcov --remove coverage.info '/usr/*' '*/tests/*' --output-file coverage.info && \
		genhtml coverage.info --output-directory coverage_html
	@echo "$(GREEN)Coverage report: $(BUILD_DIR)/coverage_html/index.html$(NC)"

##@ Code Quality

format: ## Format code with clang-format
	@echo "$(YELLOW)Formatting code...$(NC)"
	@find src include tests -name "*.cpp" -o -name "*.h" | xargs clang-format -i
	@echo "$(GREEN)Code formatted$(NC)"

format-check: ## Check code formatting
	@echo "$(YELLOW)Checking code format...$(NC)"
	@find src include tests -name "*.cpp" -o -name "*.h" | xargs clang-format --dry-run -Werror

lint: ## Run clang-tidy linter
	@echo "$(YELLOW)Running linter...$(NC)"
	@cd $(BUILD_DIR) && cmake -DCMAKE_EXPORT_COMPILE_COMMANDS=ON .. && \
		run-clang-tidy -p . || true

check: format-check lint ## Run all code quality checks

##@ Docker - Development

docker-build: ## Build Docker images for all validators
	@echo "$(YELLOW)Building Docker images...$(NC)"
	@$(DOCKER_COMPOSE) build
	@echo "$(GREEN)Docker images built$(NC)"

docker-build-dev: ## Build development Docker image
	@echo "$(YELLOW)Building development Docker image...$(NC)"
	@docker build -f docker/Dockerfile.dev -t sarafu-dev .
	@echo "$(GREEN)Development image built$(NC)"

docker-up: ## Start the testnet (10 validators)
	@echo "$(YELLOW)Starting Sarafu testnet...$(NC)"
	@$(DOCKER_COMPOSE) up -d
	@echo "$(GREEN)Testnet started$(NC)"
	@echo "$(BLUE)Waiting for validators to start...$(NC)"
	@sleep 5
	@$(MAKE) docker-status

docker-down: ## Stop the testnet (preserves data)
	@echo "$(YELLOW)Stopping testnet...$(NC)"
	@$(DOCKER_COMPOSE) down
	@echo "$(GREEN)Testnet stopped$(NC)"

docker-restart: docker-down docker-up ## Restart the testnet

docker-logs: ## View logs from all validators
	@$(DOCKER_COMPOSE) logs -f

docker-logs-%: ## View logs from specific validator (e.g., make docker-logs-1)
	@$(DOCKER_COMPOSE) logs -f validator-$*

docker-status: ## Check status of all validators
	@echo "$(BLUE)Container Status:$(NC)"
	@$(DOCKER_COMPOSE) ps
	@echo ""
	@echo "$(BLUE)Validator Health Checks:$(NC)"
	@for i in $$(seq 1 $(VALIDATOR_COUNT)); do \
		PORT=$$((8079 + i)); \
		if curl -s -f "http://localhost:$$PORT/health" > /dev/null 2>&1; then \
			echo "$(GREEN)✓$(NC) validator-$$i (http://localhost:$$PORT) - HEALTHY"; \
		else \
			echo "$(RED)✗$(NC) validator-$$i (http://localhost:$$PORT) - UNHEALTHY"; \
		fi; \
	done

docker-clean: ## Remove all containers, volumes, and images
	@echo "$(YELLOW)Cleaning Docker resources...$(NC)"
	@$(DOCKER_COMPOSE) down -v --rmi local
	@echo "$(GREEN)Docker resources cleaned$(NC)"

docker-reset: docker-clean docker-build docker-up ## Complete reset: clean, rebuild, and start

docker-shell-%: ## Open shell in validator container (e.g., make docker-shell-1)
	@docker exec -it sarafu-validator-$* /bin/bash

##@ Local Node

run-local: build keys-local genesis-local ## Run a local single node
	@echo "$(YELLOW)Starting local node...$(NC)"
	@cd $(BUILD_DIR) && ./sarafu-node --config ../config.local.toml --log-level INFO

run-validator: build keys-validator ## Run as validator node
	@echo "$(YELLOW)Starting validator node...$(NC)"
	@cd $(BUILD_DIR) && ./sarafu-node --config ../config.testnet.toml --validator --log-level INFO

run-mainnet: build ## Run mainnet node
	@echo "$(YELLOW)Starting mainnet node...$(NC)"
	@cd $(BUILD_DIR) && ./sarafu-node --config ../config.mainnet.toml --log-level INFO

##@ Keys & Genesis

keys-local: ## Generate local development keys
	@echo "$(YELLOW)Generating local development keys...$(NC)"
	@mkdir -p $(BUILD_DIR)/keys
	@echo '{"type":"BLS12-381","private_key":"0x1234567890abcdef1234567890abcdef1234567890abcdef1234567890abcdef","public_key":"0xabcdef1234567890abcdef1234567890abcdef1234567890abcdef1234567890abcdef1234567890abcdef1234567890","created_at":"2026-02-20T00:00:00Z"}' > $(BUILD_DIR)/keys/validator_consensus.key
	@echo '{"type":"Ed25519","private_key":"0xfedcba0987654321fedcba0987654321fedcba0987654321fedcba0987654321","public_key":"0x1234567890abcdef1234567890abcdef1234567890abcdef1234567890abcdef","created_at":"2026-02-20T00:00:00Z"}' > $(BUILD_DIR)/keys/validator_withdrawal.key
	@echo "$(GREEN)Keys generated in $(BUILD_DIR)/keys/$(NC)"

keys-validator: ## Generate validator keys (production-ready)
	@echo "$(YELLOW)Generating validator keys...$(NC)"
	@mkdir -p keys
	@cd $(BUILD_DIR) && ./sarafu-node keygen --output ../keys/
	@echo "$(GREEN)Keys generated in keys/$(NC)"
	@echo "$(RED)⚠️  IMPORTANT: Backup these keys securely!$(NC)"

genesis-local: ## Create local genesis file
	@echo "$(YELLOW)Creating local genesis file...$(NC)"
	@mkdir -p $(BUILD_DIR)
	@echo '{"chain_id":"sarafu-local-1","genesis_time":"2026-02-20T00:00:00Z","initial_validators":[{"consensus_pubkey":"0xabcdef1234567890abcdef1234567890abcdef1234567890abcdef1234567890abcdef1234567890abcdef1234567890","withdrawal_address":"0x1234567890abcdef1234567890abcdef1234567890abcdef1234567890abcdef","stake":"32000000000000000000"}],"initial_accounts":[{"address":"0x1111111111111111111111111111111111111111","balance":"1000000000000000000000"},{"address":"0x2222222222222222222222222222222222222222","balance":"1000000000000000000000"}],"config":{"slot_duration_ms":12000,"slots_per_epoch":32,"min_validator_stake":"32000000000000000000","max_validators":1024,"base_fee_per_gas":"1000000000"}}' | jq '.' > $(BUILD_DIR)/genesis.local.json
	@echo "$(GREEN)Genesis file created: $(BUILD_DIR)/genesis.local.json$(NC)"

##@ API Testing

api-health: ## Check node health
	@curl -s http://localhost:8080/health | jq '.' || echo "$(RED)Node not responding$(NC)"

api-info: ## Get node info
	@curl -s http://localhost:8080/api/v1/node/info | jq '.' || echo "$(RED)Endpoint not available$(NC)"

api-status: ## Get chain status
	@curl -s http://localhost:8080/api/v1/chain/status | jq '.' || echo "$(RED)Endpoint not available$(NC)"

api-validators: ## List validators
	@curl -s http://localhost:8080/api/v1/validators | jq '.' || echo "$(RED)Endpoint not available$(NC)"

api-test-all: api-health api-info api-status api-validators ## Test all API endpoints

##@ Benchmarking

bench: build ## Run benchmarks
	@echo "$(YELLOW)Running benchmarks...$(NC)"
	@cd $(BUILD_DIR) && ctest -R "bench_" --verbose

bench-consensus: build ## Benchmark consensus performance
	@cd $(BUILD_DIR) && ./tests/bench_consensus

bench-crypto: build ## Benchmark cryptographic operations
	@cd $(BUILD_DIR) && ./tests/bench_crypto

##@ Monitoring

logs-local: ## Tail local node logs
	@tail -f $(BUILD_DIR)/data/local/node.log

logs-testnet: ## Tail testnet logs
	@tail -f data/testnet/node.log

metrics: ## Show node metrics
	@curl -s http://localhost:8080/metrics || echo "$(RED)Metrics not available$(NC)"

##@ Utilities

version: ## Show version information
	@cd $(BUILD_DIR) && ./sarafu-node --version 2>/dev/null || echo "$(RED)Build the project first: make build$(NC)"

info: ## Show build information
	@echo "$(BLUE)Sarafu Blockchain Build Information$(NC)"
	@echo "Build Directory: $(BUILD_DIR)"
	@echo "Build Type: $(BUILD_TYPE)"
	@echo "Processors: $(NPROC)"
	@echo "CMake Flags: $(CMAKE_FLAGS)"
	@echo ""
	@echo "$(BLUE)Docker Configuration$(NC)"
	@echo "Validator Count: $(VALIDATOR_COUNT)"
	@echo "Compose File: docker/docker-compose.yml"

endpoints: ## Show all validator endpoints
	@echo "$(BLUE)Validator Endpoints:$(NC)"
	@for i in $$(seq 1 $(VALIDATOR_COUNT)); do \
		REST_PORT=$$((8079 + i)); \
		GRPC_PORT=$$((9089 + i)); \
		P2P_PORT=$$((26655 + i)); \
		echo "Validator $$i:"; \
		echo "  REST: http://localhost:$$REST_PORT"; \
		echo "  gRPC: localhost:$$GRPC_PORT"; \
		echo "  P2P:  localhost:$$P2P_PORT"; \
	done

watch-blocks: ## Watch new blocks in real-time
	@watch -n 2 'curl -s http://localhost:8080/api/v1/chain/status | jq ".latest_block"'

##@ Development

dev-setup: deps build keys-local genesis-local ## Complete development setup
	@echo "$(GREEN)Development environment ready!$(NC)"
	@echo "Run '$(BLUE)make run-local$(NC)' to start a local node"

dev-shell: docker-build-dev ## Start interactive development shell
	@docker run -it --rm \
		-v $$(pwd):/workspace \
		-p 26656:26656 -p 9090:9090 -p 8080:8080 \
		sarafu-dev /bin/bash

quick-test: build test-unit ## Quick test (unit tests only)

full-test: build test test-integration ## Full test suite

ci: clean build test lint ## CI pipeline: clean, build, test, lint

##@ Documentation

docs: ## Generate documentation
	@echo "$(YELLOW)Generating documentation...$(NC)"
	@doxygen Doxyfile 2>/dev/null || echo "$(RED)Doxygen not installed$(NC)"

docs-serve: docs ## Serve documentation locally
	@cd docs/_build/html && python3 -m http.server 8000

##@ Maintenance

update-deps: ## Update dependencies
	@echo "$(YELLOW)Updating dependencies...$(NC)"
	@git submodule update --init --recursive
	@cd $(BUILD_DIR) && cmake ..

check-deps: ## Check for missing dependencies
	@echo "$(YELLOW)Checking dependencies...$(NC)"
	@command -v cmake >/dev/null 2>&1 || echo "$(RED)✗ cmake not found$(NC)"
	@command -v g++ >/dev/null 2>&1 || echo "$(RED)✗ g++ not found$(NC)"
	@command -v protoc >/dev/null 2>&1 || echo "$(RED)✗ protoc not found$(NC)"
	@command -v docker >/dev/null 2>&1 || echo "$(RED)✗ docker not found$(NC)"
	@command -v docker-compose >/dev/null 2>&1 || echo "$(RED)✗ docker-compose not found$(NC)"
	@pkg-config --exists libsodium || echo "$(RED)✗ libsodium not found$(NC)"
	@pkg-config --exists protobuf || echo "$(RED)✗ protobuf not found$(NC)"
	@echo "$(GREEN)Dependency check complete$(NC)"

backup-data: ## Backup blockchain data
	@echo "$(YELLOW)Backing up data...$(NC)"
	@tar -czf backup-$$(date +%Y%m%d-%H%M%S).tar.gz data/ keys/
	@echo "$(GREEN)Backup created$(NC)"

##@ Quick Commands

start: docker-up ## Quick start testnet

stop: docker-down ## Quick stop testnet

restart: docker-restart ## Quick restart testnet

status: docker-status ## Quick status check

shell: docker-shell-1 ## Quick shell into validator-1
