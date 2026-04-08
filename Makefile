# UC2-ESP Makefile — convenience targets
#
# Usage:
#   make canopen-regen    Re-generate all CANopen artifacts from the registry
#   make canopen-check    Run the generator and verify no files changed (CI-style)

PYTHON ?= python3
REGISTRY = tools/canopen/uc2_canopen_registry.yaml
GENERATOR = tools/canopen/regenerate_all.py

.PHONY: canopen-regen canopen-check

canopen-regen: ## Regenerate all CANopen artifacts from uc2_canopen_registry.yaml
	$(PYTHON) $(GENERATOR)

canopen-check: ## Regenerate and fail if any generated file differs (drift check)
	$(PYTHON) $(GENERATOR)
	git diff --exit-code || { \
		echo ""; \
		echo "ERROR: Generated files are out of date with $(REGISTRY)."; \
		echo "Run 'make canopen-regen' and commit the results."; \
		exit 1; \
	}

help: ## Show this help
	@grep -E '^[a-zA-Z_-]+:.*##' $(MAKEFILE_LIST) | \
		awk 'BEGIN {FS = ":.*##"}; {printf "  %-20s %s\n", $$1, $$2}'
