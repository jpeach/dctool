include build/colors.mk

.PHONY: help
help: ## Display this help text
	@grep -h -E '^[a-zA-Z0-9/_-]+:.*?## .*$$' $(MAKEFILE_LIST) | \
		sort -k1 | \
		awk 'BEGIN {FS = ":.*?## "}; {printf "%s%-30s%s%s\n", $(Color_Cyan), $$1, $(Color_Reset), $$2}'

.PHONY: serial/dc-tool
serial/dc-tool: ### Build the serial version of dc-tool
	$(MAKE) -C serial/host-src/tool

.PHONY: ip/dc-tool
ip/dc-tool: ### Build the IP version of dc-tool
	$(MAKE) -C ip/host-src/tool

.PHONY: clean
clean: ### Remove intermediate build artifacts
	$(MAKE) -C ip clean
	$(MAKE) -C serial clean

.PHONY: distclean
distclean: ### Remove all build artifacts
	$(MAKE) -C ip clean
	$(MAKE) -C serial clean


