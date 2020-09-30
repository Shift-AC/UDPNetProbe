# ShiftAC's C/C++ makefile template for mkcproj 1.3

TARGET := ../bin
PACKAGE_PREFIX := UDPNetProbe
CC := gcc
CXX := g++
CMACRO := 
CXXMACRO := $(CMACRO)
COMMONFLAGS := -I include -g -Og
CFLAGS := $(CMACRO) $(COMMONFLAGS)
CXXFLAGS := $(CXXMACRO) $(COMMONFLAGS) -std=c++14
IGNORE_SRC := 
GEN_SRC := 
PROG_SRC := Sender.cc Receiver.cc
SRC := $(filter-out $(IGNORE_SRC) $(GEN_SRC) $(PROG_SRC),$(wildcard *.c) $(wildcard *.cc))
OUT := $(addsuffix .o, $(basename $(SRC) $(GEN_SRC)))
SCRIPTS_DIR := ./scripts
BIN_SCRIPT := 
LIB := -lpthread
PROGS := $(BIN_SCRIPT) $(basename $(PROG_SRC))
PROGNAMES := $(addprefix $(PACKAGE_PREFIX)-,$(PROGS))

$(PACKAGE_PREFIX)-%: %.cpp $(OUT)
	$(CXX) $(CXXFLAGS) -o $(TARGET)/$(shell echo $@ | tr '[A-Z]' '[a-z]') $< $(OUT) $(LIB)

$(PACKAGE_PREFIX)-%: %.cc $(OUT)
	$(CXX) $(CXXFLAGS) -o $(TARGET)/$(shell echo $@ | tr '[A-Z]' '[a-z]') $< $(OUT) $(LIB)

$(PACKAGE_PREFIX)-%.sh: $(SCRIPTS_DIR)/%.sh
	cp $< $(TARGET)/$@

$(PACKAGE_PREFIX)-%: %.c $(OUT)
	$(CC) $(CFLAGS) -o $(TARGET)/$(shell echo $@ | tr '[A-Z]' '[a-z]') $< $(OUT) $(LIB)

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $<

%.o: %.cc
	$(CXX) $(CXXFLAGS) -c $<

%.o: %.c
	$(CC) $(CFLAGS) -c $<

%.cpp:
	$(SCRIPTS_DIR)/generate$@.sh > $@

%.cc:
	$(SCRIPTS_DIR)/generate$@.sh > $@

%.c:
	$(SCRIPTS_DIR)/generate$@.sh > $@
	
.PHONY: all
all: depend $(GEN_SRC) $(OUT) $(PROGNAMES)

.PHONY: sourceClean
sourceClean:
	-rm $(GEN_SRC)

.PHONY: clean
clean: sourceClean
	-rm $(OUT) $(PROGNAMES) *~ Makefile.dep 2> /dev/null

.PHONY: linecount
linecount: clean
	-find | xargs cat 2> /dev/null | wc -l

depend:
	-makedepend -f- -- $(CFLAGS) -- $(SRC) > Makefile.dep 2> /dev/null

# make `make` happy...
Makefile.dep:
	touch Makefile.dep

-include Makefile.dep
