include ../Makefile.config

INCLUDE += -I../
LDFLAGS += -L../
LDLIBS  += -l:CloudLib.DEM/$(OBJDIR)/dem.a \
           -l:CloudTools.Common/$(OBJDIR)/common.a \
           -lboost_system -lboost_filesystem \
           -lboost_program_options \
           -lboost_regex \
           -lgdal

DEPS    := $(wildcard *.h)
DEPS    += $(wildcard *.hpp)
SRCS     = $(wildcard *.cpp)
OBJS     = $(SRCS:.cpp=.o)
MAIN     = ahn_buildings_ver

OBJECTS  = $(addprefix $(OBJDIR)/, $(OBJS))
TARGET   = $(OBJDIR)/$(MAIN)

.PHONY: all
all: $(TARGET)

$(TARGET): $(OBJDIR) $(OBJECTS)
	$(CXX) $(CFLAGS) $(OBJECTS) -o $(TARGET) $(LDFLAGS) $(LDLIBS)

$(OBJDIR):
	$(MKDIR) $(OBJDIR)

$(OBJDIR)/%.o: %.cpp $(DEPS)
	$(CXX) $(CFLAGS) $(INCLUDE) -c $< -o $@ 

.PHONY: clean
clean:
	$(RM) $(TARGET) $(OBJECTS)
