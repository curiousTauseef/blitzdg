CXX := $(or $(CXX), g++)
SRCDIR := src
BUILDDIR := build
BINDIR := bin
TARGET := $(BINDIR)/advec1d
TESTTARGET := $(BINDIR)/test

SRCEXT := cpp
SOURCES := $(wildcard $(SRCDIR)/*.$(SRCEXT))
SOURCES += $(wildcard $(SRCDIR)/**/*.$(SRCEXT))
ALLOBJECTS := $(patsubst $(SRCDIR)/%,$(BUILDDIR)/%,$(SOURCES:.$(SRCEXT)=.o))
OBJECTS := $(patsubst build/test/tests.o,,$(ALLOBJECTS))
TESTOBJECTS := $(patsubst build/advec1d/main.o,,$(ALLOBJECTS))

CFLAGS := -g -Wall -std=c++0x -fprofile-arcs -ftest-coverage
LINKERFLAGS := -fprofile-arcs
INC := -I include -I /usr/include
LIB := -L lib -lblitz -lmetis -lumfpack -lcxsparse -llapack -lblas
EXPLICITLIBS := -lgfortran -lcholmod -lamd -lcolamd -lquadmath -lsuitesparseconfig

ifeq ($(OS), Windows_NT)
	LIB += $(EXPLICITLIBS)
endif

$(TARGET): $(OBJECTS) $(TESTTARGET)
	@mkdir -p bin
	@echo " Linking main binary..."
	@echo " $(CXX) $(LINKERFLAGS) $(OBJECTS) -o $(TARGET) $(LIB)"; $(CXX) $(LINKERFLAGS) $(OBJECTS) -o $(TARGET) $(LIB)

$(TESTTARGET): $(TESTOBJECTS)
	@mkdir -p bin
	@echo " Linking tests..."
	@echo " $(CXX) $(LINKERFLAGS) $(TESTOBJECTS) -o $(TESTTARGET) $(LIB)"; $(CXX) $(LINKERFLAGS) $(TESTOBJECTS) -o $(TESTTARGET) $(LIB)

$(BUILDDIR)/%.o: $(SRCDIR)/%.$(SRCEXT)
	@mkdir -p $(BUILDDIR)/test
	@mkdir -p $(BUILDDIR)/advec1d
	@echo " Building...";
	@echo " $(CXX) $(CFLAGS) $(EXTRACFLAGS) $(INC) -c -o $@ $<"; $(CXX) $(CFLAGS) $(EXTRACFLAGS) $(INC) -c -o $@ $<

clean:
	@echo " Cleaning...";
	@echo " $(RM) -r $(BUILDDIR) $(BINDIR)"; $(RM) -r $(BUILDDIR) $(BINDIR)

test: bin/test
	@bin/test

get-deps:
	@sudo /bin/bash pull-deps.sh
	@sudo apt-get -y install graphviz texlive-generic-recommended

docs:
	@echo DOXYGEN VERSION:
	@doxygen -v
	@doxygen -u doxygen.conf
	@sed -ir "s,DOT_PATH.*=,DOT_PATH               = $$(which dot)," doxygen.conf
	@cat doxygen.conf | grep DOT_PATH
	@doxygen doxygen.conf
.PHONY: clean docs
