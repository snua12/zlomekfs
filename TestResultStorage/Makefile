
all: build

PREFIX ?= /usr/local

build:
	./setup.py build

install:
	./setup.py install --prefix "$(DESTDIR)/$(PREFIX)"

clean:
	rm -rf MANIFEST build dist *.pyc resultRepository/*.pyc