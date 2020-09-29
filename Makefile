# ShiftAC's C/C++ makefile template for mkcproj 1.2

INSTALL_PATH := /bin

.PHONY: all
all: init clean
	make -C src

.PHONY: init
init:
	-mkdir bin
	-mkdir temp

.PHONY: install
install:
	cp bin/* $(INSTALL_PATH)/

.PHONY: uninstall
uninstall:
	-rm $(INSTALL_PATH)/UDPNetProbe-*

.PHONY: clean
clean:
	-rm bin/* -r
	-rm temp/* -r
	make -C src clean
