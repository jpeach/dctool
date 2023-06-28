include build/colors.mk

.PHONY: help
help: ## Display this help text
	@grep -h -E '^[a-zA-Z0-9/_-]+:.*?## .*$$' $(MAKEFILE_LIST) | \
		sort -k1 | \
		awk 'BEGIN {FS = ":.*?## "}; {printf "%s%-30s%s%s\n", $(Color_Cyan), $$1, $(Color_Reset), $$2}'

.PHONY: dc-tool
dc-tool: ### Build dc-tool
	$(MAKE) -C host-src/dc-tool

.PHONY: ip/dcload
ip/dcload: ### Build dcload for IP connections
	$(MAKE) -C ip/target-src/dcload

.PHONY: serial/dcload
serial/dcload: ### Build dcload for serial connections
	$(MAKE) -C serial/target-src/dcload

SUBDIRS := ip serial host-src/dc-tool

.PHONY: clean
clean: ### Remove intermediate build artifacts
	for dir in $(SUBDIRS); do \
		$(MAKE) -C $$dir clean; \
	done

.PHONY: distclean
distclean: ### Remove all build artifacts
	for dir in $(SUBDIRS); do \
		$(MAKE) -C $$dir distclean; \
	done


