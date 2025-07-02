# MCP Debugger Makefile
# Cross-platform build automation

.PHONY: all build test clean install package help
.DEFAULT_GOAL := help

# Configuration
PLATFORM ?= x64
CONFIG ?= Release
PROJECT_ROOT := $(shell pwd)
SCRIPTS_DIR := $(PROJECT_ROOT)/scripts

# Colors for output
GREEN := \033[32m
YELLOW := \033[33m
RED := \033[31m
BLUE := \033[34m
NC := \033[0m # No Color

help: ## Show this help message
	@echo "$(BLUE)MCP Debugger Build System$(NC)"
	@echo "========================="
	@echo ""
	@echo "Available targets:"
	@grep -E '^[a-zA-Z_-]+:.*?## .*$$' $(MAKEFILE_LIST) | sort | awk 'BEGIN {FS = ":.*?## "}; {printf "  $(GREEN)%-15s$(NC) %s\n", $$1, $$2}'
	@echo ""
	@echo "Configuration:"
	@echo "  PLATFORM: $(PLATFORM) (x64, x86)"
	@echo "  CONFIG:   $(CONFIG) (Debug, Release)"

all: build test package ## Build, test, and package the project

build: ## Build the project
	@echo "$(GREEN)Building MCP Debugger ($(PLATFORM), $(CONFIG))...$(NC)"
ifeq ($(OS),Windows_NT)
	@powershell -ExecutionPolicy Bypass -File "$(SCRIPTS_DIR)/build_system.ps1" build -Platform $(PLATFORM) -Config $(CONFIG)
else
	@echo "$(YELLOW)Warning: Building on non-Windows platform. Some features may not work.$(NC)"
	@mkdir -p build/$(PLATFORM)
	@cd build/$(PLATFORM) && cmake -DCMAKE_BUILD_TYPE=$(CONFIG) -DBUILD_TESTS=ON ../..
	@cd build/$(PLATFORM) && cmake --build . --config $(CONFIG) --parallel 4
endif

test: build ## Run all tests
	@echo "$(GREEN)Running tests...$(NC)"
ifeq ($(OS),Windows_NT)
	@python "$(SCRIPTS_DIR)/run_integration_tests.py" --platform $(PLATFORM) --config $(CONFIG)
else
	@cd build/$(PLATFORM) && ctest --output-on-failure --build-config $(CONFIG)
endif

test-unit: build ## Run only unit tests
	@echo "$(GREEN)Running unit tests...$(NC)"
ifeq ($(OS),Windows_NT)
	@cd build/$(PLATFORM) && ctest --output-on-failure --build-config $(CONFIG)
else
	@cd build/$(PLATFORM) && ctest --output-on-failure --build-config $(CONFIG)
endif

test-integration: build ## Run integration tests
	@echo "$(GREEN)Running integration tests...$(NC)"
	@python "$(SCRIPTS_DIR)/run_integration_tests.py" --platform $(PLATFORM) --config $(CONFIG)

test-performance: build ## Run performance tests
	@echo "$(GREEN)Running performance tests...$(NC)"
	@python "$(SCRIPTS_DIR)/performance_tests.py" --binary "build/$(PLATFORM)/src/cli/$(CONFIG)/mcp-debugger.exe" --iterations 50

security-scan: ## Run security scan
	@echo "$(GREEN)Running security scan...$(NC)"
	@python "$(SCRIPTS_DIR)/security_scan.py" --project-root "$(PROJECT_ROOT)"

package: build ## Create distribution packages
	@echo "$(GREEN)Creating packages...$(NC)"
	@python "$(SCRIPTS_DIR)/package.py" --platform $(PLATFORM) --config $(CONFIG) --output artifacts

clean: ## Clean build artifacts
	@echo "$(GREEN)Cleaning build artifacts...$(NC)"
ifeq ($(OS),Windows_NT)
	@powershell -ExecutionPolicy Bypass -File "$(SCRIPTS_DIR)/build_system.ps1" clean
else
	@rm -rf build artifacts reports logs
endif

install: package ## Install the application
	@echo "$(GREEN)Installing MCP Debugger...$(NC)"
ifeq ($(OS),Windows_NT)
	@echo "Run: artifacts/MCP-Debugger-1.0.0-$(PLATFORM)/install.bat"
else
	@echo "$(YELLOW)Installation not supported on this platform$(NC)"
endif

ci: ## Run full CI/CD pipeline
	@echo "$(GREEN)Running CI/CD pipeline...$(NC)"
	@python "$(SCRIPTS_DIR)/ci_cd_manager.py" --platform $(PLATFORM) --config $(CONFIG)

ci-quick: ## Run quick CI pipeline (skip performance tests)
	@echo "$(GREEN)Running quick CI pipeline...$(NC)"
	@python "$(SCRIPTS_DIR)/ci_cd_manager.py" --platform $(PLATFORM) --config $(CONFIG) --skip-performance

setup-deps: ## Setup development dependencies (Windows)
	@echo "$(GREEN)Setting up dependencies...$(NC)"
ifeq ($(OS),Windows_NT)
	@powershell -ExecutionPolicy Bypass -File "$(SCRIPTS_DIR)/install-deps-windows.ps1"
else
	@echo "$(YELLOW)Dependency setup only supported on Windows$(NC)"
endif

debug: ## Build debug version
	@$(MAKE) build CONFIG=Debug

release: ## Build release version
	@$(MAKE) build CONFIG=Release

x86: ## Build for x86
	@$(MAKE) build PLATFORM=x86

x64: ## Build for x64
	@$(MAKE) build PLATFORM=x64

format: ## Format source code (placeholder)
	@echo "$(GREEN)Formatting source code...$(NC)"
	@echo "$(YELLOW)Code formatting not implemented yet$(NC)"

lint: ## Run code linting (placeholder)
	@echo "$(GREEN)Running code linter...$(NC)"
	@echo "$(YELLOW)Code linting not implemented yet$(NC)"

docs: ## Generate documentation (placeholder)
	@echo "$(GREEN)Generating documentation...$(NC)"
	@echo "$(YELLOW)Documentation generation not implemented yet$(NC)"

version: ## Show version information
	@echo "$(BLUE)MCP Debugger Build System$(NC)"
	@echo "Version: 1.0.0"
	@echo "Platform: $(PLATFORM)"
	@echo "Configuration: $(CONFIG)"
	@echo "Project Root: $(PROJECT_ROOT)"

env: ## Show environment information
	@echo "$(BLUE)Environment Information$(NC)"
	@echo "======================="
	@echo "OS: $(OS)"
	@echo "Platform: $(PLATFORM)"
	@echo "Configuration: $(CONFIG)"
	@echo "Project Root: $(PROJECT_ROOT)"
	@echo "Scripts Directory: $(SCRIPTS_DIR)"
	@echo ""
	@echo "Available Tools:"
ifeq ($(OS),Windows_NT)
	@where cmake 2>nul && echo "  ✓ CMake" || echo "  ✗ CMake"
	@where cl 2>nul && echo "  ✓ MSVC" || echo "  ✗ MSVC"
	@where python 2>nul && echo "  ✓ Python" || echo "  ✗ Python"
	@where git 2>nul && echo "  ✓ Git" || echo "  ✗ Git"
else
	@which cmake >/dev/null 2>&1 && echo "  ✓ CMake" || echo "  ✗ CMake"
	@which gcc >/dev/null 2>&1 && echo "  ✓ GCC" || echo "  ✗ GCC"
	@which python3 >/dev/null 2>&1 && echo "  ✓ Python" || echo "  ✗ Python"
	@which git >/dev/null 2>&1 && echo "  ✓ Git" || echo "  ✗ Git"
endif

# Development shortcuts
dev: debug test-unit ## Quick development build and test

prod: release test package ## Production build with full testing

# Platform-specific targets
windows: ## Build for Windows (both x64 and x86)
	@$(MAKE) build PLATFORM=x64 CONFIG=Release
	@$(MAKE) build PLATFORM=x86 CONFIG=Release

# Multi-configuration builds
all-configs: ## Build all platform/config combinations
	@$(MAKE) build PLATFORM=x64 CONFIG=Debug
	@$(MAKE) build PLATFORM=x64 CONFIG=Release
	@$(MAKE) build PLATFORM=x86 CONFIG=Debug
	@$(MAKE) build PLATFORM=x86 CONFIG=Release