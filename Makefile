clean:
	@$(MAKE) -C insecticide clean
	@$(MAKE) -C zfs clean
	@$(MAKE) -C TestResultStorage clean
	@$(MAKE) -C zen-unit clean
	@$(MAKE) -C syplog clean

