CXX     = g++
RM      = rm -f
MKDIR   = mkdir -p

OBJDIR  = Release/Linux

CFLAGS  = -O2 -std=c++11 -pedantic
INCLUDE = -I../
LDFLAGS = -L../
LDLIBS  = -l:CloudLib.DEM/$(OBJDIR)/dem.a \
          -l:CloudTools.Common/$(OBJDIR)/common.a \
          -lboost_system -lboost_filesystem \
          -lboost_program_options \
          -lgdal

DEPS   := $(wildcard *.h)
DEPS   += $(wildcard *.hpp)
SRCS    = $(wildcard *.cpp)
OBJS    = $(SRCS:.cpp=.o)
MAIN    = dem_filter

OBJECTS = $(addprefix $(OBJDIR)/, $(OBJS))
TARGET  = $(OBJDIR)/$(MAIN)

all: dirs $(TARGET)

.PHONY: dirs
dirs:
	$(MKDIR) $(OBJDIR)

$(TARGET): $(OBJECTS)
	$(CXX) $(CFLAGS) $(OBJECTS) -o $(TARGET) $(LDFLAGS) $(LDLIBS)

$(OBJDIR)/%.o: %.cpp $(DEPS)
	$(CXX) $(CFLAGS) $(INCLUDE) -c $< -o $@ 

clean:
	$(RM) $(TARGET) $(OBJECTS)
