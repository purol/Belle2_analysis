CXX = g++ -std=c++14
RM = rm -rf
CP = cp -f

BINDIR = ./bin
INCDIR = ./include
SRCDIR = ./src
TMPDIR = ./tmp
LIBDIR = ./lib
BDTINC = ./include_FBDT

CFLAGS = `root-config --cflags`
LDFLAGS = `root-config --ldflags --glibs`
LIBS = -lRooFit -lRooStats -lRooFitCore  -lMinuit -lFastBDT_static
#LIBS += -lTMVA -lTMVAGui
INCLUDES = -I$(INCDIR) -I$(BDTINC)

MAINS = $(wildcard *.cc)
TARGETS = $(MAINS:%.cc=$(BINDIR)/%)

SRCS = $(wildcard $(SRCDIR)/*.cc)
OBJS = $(MAINS:%.cc=$(TMPDIR)/%.o)
#OBJS = $(SRCS:$(SRCDIR)/%.cc=$(TMPDIR)/%.o)

.PHONY : all clean

all: $(TARGETS)

force:
	@ $(RM) $(BINDIR)/* $(TMPDIR)/*
	@ make all

clean:
	@ echo '<< cleaning directory >>'
	@ $(RM) *~ */*~ \#*\#* */\#*\#*
	@ $(RM) $(BINDIR)/* $(TMPDIR)/*

$(TARGETS): $(BINDIR)/% : $(TMPDIR)/%.o
	@ echo '<< creating executable $@ >>'
	@ $(CXX) -o $@ $< $(LDFLAGS) $(LIBS) $(INCLUDES) -L$(LIBDIR)
	@ $(RM) *~ */*~ \#*\#* */\#*\#*
	@ echo '<< compiling succeded!! >>'

$(OBJS): $(TMPDIR)/%.o : %.cc
	@ echo '<< compiling $@ >>'
	@ $(CXX) $(CFLAGS) $(INCLUDES) -L$(LIBDIR) -c $< -o $@



