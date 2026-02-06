# --------------------------------------------------
# Project configuration
# --------------------------------------------------

PROJECT_NAME := {{ cookiecutter.project_name }}
SRC_DIR := src
DOCS_DIR := docs
DOCS_BUILD := docs/build/html
TESTS_DIR := tests

release: ## Build Release using CMake (VERSION=2026)
build:   ## Build Debug using CMake (VERSION=2026)

# Python executable (Maya)
MAYAPY ?= mayapy

# --------------------------------------------------
# Platform detection
# --------------------------------------------------

ifeq ($(OS),Windows_NT)
    PLATFORM := windows
    PATH_SEP := ;
    OPEN_CMD := start
    SET_PYTHONPATH = set PYTHONPATH=$(CURDIR)\$(SRC_DIR)$(PATH_SEP)%PYTHONPATH% &&
else
    PLATFORM := unix
    PATH_SEP := :
    OPEN_CMD := xdg-open
    SET_PYTHONPATH = PYTHONPATH=$(CURDIR)/$(SRC_DIR)$(PATH_SEP)$$PYTHONPATH
endif

# --------------------------------------------------
# Help system
# --------------------------------------------------

.PHONY: help
help: ## Show available commands
	@echo ""
	@echo "Usage: make <target>"
	@echo ""
	@grep -E '^[a-zA-Z_-]+:.*?## ' $(MAKEFILE_LIST) \
		| sort \
		| awk 'BEGIN {FS = ":.*?## "}; {printf "  \033[36m%-22s\033[0m %s\n", $$1, $$2}'

# --------------------------------------------------
# Documentation
# --------------------------------------------------

.PHONY: docs
docs: ## Build Sphinx documentation
ifeq ($(PLATFORM),windows)
	cd $(DOCS_DIR) && make html
else
	cd $(DOCS_DIR) && make html
endif

.PHONY: show-doc
show-doc: ## Open built documentation in browser
	$(OPEN_CMD) $(DOCS_BUILD)/index.html

# --------------------------------------------------
# Tests
# --------------------------------------------------

.PHONY: tests
tests: tests-unit tests-integration ## Run all tests

.PHONY: tests-unit
tests-unit: ## Run unit tests
	$(SET_PYTHONPATH) $(MAYAPY) $(TESTS_DIR)/unit/invoke.py

.PHONY: tests-integration
tests-integration: ## Run integration tests
	$(SET_PYTHONPATH) $(MAYAPY) $(TESTS_DIR)/integration/invoke.py

# --------------------------------------------------
# Coverage
# --------------------------------------------------

.PHONY: tests-cov
tests-cov: ## Run all tests with coverage
	$(MAYAPY) -m coverage erase
	$(SET_PYTHONPATH) $(MAYAPY) -m coverage run $(TESTS_DIR)/unit/invoke.py
	$(SET_PYTHONPATH) $(MAYAPY) -m coverage run $(TESTS_DIR)/integration/invoke.py
	$(MAYAPY) -m coverage report

.PHONY: tests-cov-unit
tests-cov-unit: ## Run unit tests with coverage
	$(SET_PYTHONPATH) $(MAYAPY) -m coverage run $(TESTS_DIR)/unit/invoke.py

.PHONY: tests-cov-integration
tests-cov-integration: ## Run integration tests with coverage
	$(SET_PYTHONPATH) $(MAYAPY) -m coverage run $(TESTS_DIR)/integration/invoke.py

# --------------------------------------------------
# Build / CMake
# --------------------------------------------------

BUILD_DIR := build

ifndef VERSION
VERSION_ERROR = \
	@echo "ERROR: VERSION is required."; \
	@echo "Usage:"; \
	@echo "  make build VERSION=2026"; \
	@echo "  make release VERSION=2026"; \
	exit 1
endif

.PHONY: build
build: ## Configure and build Debug (requires VERSION)
ifndef VERSION
	$(VERSION_ERROR)
endif
	cmake -B $(BUILD_DIR) \
		-DMAYA_VERSION=$(VERSION) \
		-DCMAKE_BUILD_TYPE=Debug
	cmake --build $(BUILD_DIR) --config Debug

.PHONY: release
release: ## Configure and build Release (requires VERSION)
ifndef VERSION
	$(VERSION_ERROR)
endif
	cmake -B $(BUILD_DIR) \
		-DMAYA_VERSION=$(VERSION) \
		-DCMAKE_BUILD_TYPE=Release
	cmake --build $(BUILD_DIR) --config Release
