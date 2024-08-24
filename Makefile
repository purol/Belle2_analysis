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

SRCS = $(wildcard $(SRCDIR)/*.cc)
OBJS = $(SRCS:$(SRCDIR)/%.cc=$(TMPDIR)/%.o)
TARGETS = $(SRCS:$(SRCDIR)/%.cc=$(BINDIR)/%)

.PHONY : all clean

all: directories $(TARGETS)

directories:
	mkdir -p $(BINDIR) $(TMPDIR) $(LIBDIR)

clean:
	echo '<< cleaning directory >>'
	$(RM) *~ */*~ \#*\#* */\#*\#*
	$(RM) $(BINDIR)/* $(TMPDIR)/*

$(TARGETS): $(BINDIR)/% : $(TMPDIR)/%.o
	echo '<< creating executable $@ >>'
	$(CXX) -o $@ $< $(LDFLAGS) $(LIBS) $(INCLUDES) -L$(LIBDIR)
	echo '<< compilation succeeded! >>'

$(OBJS): $(TMPDIR)/%.o : $(SRCDIR)/%.cc
	echo '<< compiling $@ >>'
	$(CXX) $(CFLAGS) $(INCLUDES) -L$(LIBDIR) -c $< -o $@
