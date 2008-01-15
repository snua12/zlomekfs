.PHONY: all bin doc clean

all: bin doc

bin:
	@$(MAKE) -C src

doc:
	@$(MAKE) -C doc

install:
	@$(MAKE) -C src

test:
	@$(MAKE) -C src

clean:
	@$(MAKE) -C src clean
	@$(MAKE) -C doc clean

