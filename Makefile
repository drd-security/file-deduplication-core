.PHONY: all test clean
all:
	$(MAKE) -C tests

test:
	$(MAKE) -C tests run

clean:
	$(MAKE) -C tests clean
