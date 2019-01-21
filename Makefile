# Copyright (C) 2017-2019  Waterloo Quantitative Consulting Group, Inc.
# See COPYING and LICENSE files at project root for more details.

CXX := $(or $(CXX), g++)
SRCDIR := src
BUILDDIR := build
BINDIR := bin
LIB := $(LIB)
INC := $(INC)

SRCEXT := cpp
DEPEXT := d
OBJECT := o

.DEFAULT_GOAL := all

TARGETS := $(patsubst src/,,$(patsubst src/%/,bin/%,$(sort $(dir $(wildcard src/**/)))))
BUILDDIRS := $(subst bin,build,$(TARGETS))
ALLVALGRINDTARGETS :=$(patsubst bin/%,artifacts/%,$(TARGETS:%=%.log))
VALGRINDTARGETS := artifacts/test.log artifacts/advec1d.log artifacts/burgers1d.log artifacts/poisson1d.log

COMMONSOURCES := $(wildcard $(SRCDIR)/*.$(SRCEXT))
COMMONOBJECTS := $(patsubst $(SRCDIR)/%,$(BUILDDIR)/%,$(COMMONSOURCES:.$(SRCEXT)=.o))

SPECIFICSOURCES := $(wildcard $(SRCDIR)/**/*.$(SRCEXT))
SPECIFICOBJECTS := $(patsubst $(SRCDIR)/%,$(BUILDDIR)/%,$(SPECIFICSOURCES:.$(SRCEXT)=.o))

ALLOBJECTS := $(COMMONOBJECTS)
ALLOBJECTS += $(SPECIFICOBJECTS)

ALLSOURCES := $(COMMONSOURCES)
ALLSOURCES += $(SPECIFICSOURCES)

DEPS := $(ALLOBJECTS:%.o=%.$(DEPEXT))
DEPPATH := $(BUILDDIR)/$(*F)

.SUFFIXES:
.DELETE_ON_ERROR:

# Pull in dependencies
ifneq ($(MAKECMDGOALS), clean)
-include $(DEPS)
endif

CFLAGS := -g -Wall -std=c++0x -fprofile-arcs -ftest-coverage -DBZ_DEBUG
LINKERFLAGS := -fprofile-arcs
INC += -I include/igloo -I include
LIB += -lvtkIOXML-7.1 -lvtkCommonCore-7.1 -lvtkCommonDataModel-7.1 -lblitz -lmetis -lumfpack -lcxsparse -llapack -lblas
EXPLICITLIBS := -lvtkIOXMLParser-7.1 -lvtkCommonExecutionModel-7.1 -lvtkCommonMisc-7.1 -lvtkCommonSystem-7.1 -lvtkIOCore-7.1 -lvtkCommonTransforms-7.1 -lvtkCommonCore-7.1 -lvtkCommonMath-7.1 -lvtkexpat-7.1 -lvtksys-7.1 -lvtkzlib-7.1 -lgfortran -lcholmod -lamd -lcolamd -lquadmath -lsuitesparseconfig -lcrtdll
VALGRIND := valgrind --error-exitcode=1 --leak-check=full --track-origins=yes

ifeq ($(OS), Windows_NT)
	LIB += $(EXPLICITLIBS)
endif

ifeq ($(CXX), x86_64-w64-mingw32-g++-posix)
	INC += -I /usr/lib/gcc/x86_64-w64-mingw32/6.3-posix/include/c++/parallel/ -I /opt/blitzpp-mingw64/blitz-1.0.1
	LIB += $(EXPLICITLIBS)
	CFLAGS += -Wa,-mbig-obj
endif

all: $(TARGETS)

$(TARGETS): $(ALLOBJECTS)
	@mkdir -p $(BINDIR)
	@echo " Linking $@..."
	@echo " $(CXX) $(LINKERFLAGS) $(COMMONOBJECTS) $(wildcard $(subst bin/,,build/$@/*.o)) -o $@ $(LIB)"; $(CXX) $(LINKERFLAGS) $(COMMONOBJECTS) $(wildcard $(subst bin/,,build/$@/*.o)) -o $@ $(LIB)

$(BUILDDIR)/%.o: $(SRCDIR)/%.$(SRCEXT)
	@mkdir -p $(BUILDDIRS)
	@echo " $(CXX) $(CFLAGS) $(INC) -c -o $@ $<"; $(CXX) $(CFLAGS) $(INC) -c -o $@ $<

$(BUILDDIR)/%.$(DEPEXT): $(SRCDIR)/%.$(SRCEXT)
	@mkdir -p $(BUILDDIRS)
	@echo "Building dependency $@..."
	$(CXX) $(CFLAGS) $(INC) -MM -MF $@ -MP -MT $(DEPPATH).$(OBJEXT) -MT $(DEPPATH).$(DEPEXT) $<
	@$(RM) [0-9]

$(BUILDDIR)/%.$(DEPEXT): $(SRCDIR)/%.$(SRCEXT)
	@mkdir -p $(BUILDDIRS)
	@echo "Building dependency $@..."
	$(CXX) $(CFLAGS) $(INC) -MM -MF $@ -MP -MT $(DEPPATH).$(OBJEXT) -MT $(DEPPATH).$(DEPEXT) $<
	@$(RM) [0-9]

clean:
	@echo " Cleaning...";
	@echo " $(RM) -r $(BUILDDIR) $(BINDIR)" artifacts; $(RM) -r $(BUILDDIR) $(BINDIR) artifacts

test: $(BINDIR)/test
	@$(BINDIR)/test

artifacts/%.log: $(BINDIR)/%
	@mkdir -p artifacts
	@echo "$(VALGRIND) --log-file=$@ $<"; $(VALGRIND) --log-file="$@" $< > $@.out

valgrind: $(VALGRINDTARGETS)

valgrind-print: $(VALGRINDTARGETS)
	@cat artifacts/*

get-deps:
	@sudo /bin/bash pull-deps.sh

docs:
	@echo DOXYGEN VERSION:
	@doxygen -v
	@doxygen -u doxygen.conf
	@doxygen doxygen.conf

.PHONY: clean docs valgrind valgrind-print all
