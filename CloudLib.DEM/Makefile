CXX     = g++
AR      = ar
RM      = rm -f
MKDIR   = mkdir -p

OBJDIR  = Release/Linux

CFLAGS  = -O2 -std=c++11 -pedantic
INCLUDE = 
LDFLAGS = 
LDLIBS  =
ARFLAGS = -rvs

DEPS   := $(wildcard *.h)
DEPS   += $(wildcard *.hpp)
SRCS    = $(wildcard *.cpp)
OBJS    = $(SRCS:.cpp=.o)
MAIN    = dem.a

OBJECTS = $(addprefix $(OBJDIR)/, $(OBJS))
TARGET  = $(OBJDIR)/$(MAIN)

all: dirs $(TARGET)

.PHONY: dirs
dirs:
	$(MKDIR) $(OBJDIR)

$(TARGET): $(OBJECTS)
	$(AR) $(ARFLAGS) $(TARGET) $(OBJECTS)

$(OBJDIR)/%.o: %.cpp $(DEPS)
	$(CXX) $(CFLAGS) $(INCLUDE) -c $< -o $@ $(LDFLAGS) $(LDLIBS)

clean:
	$(RM) $(TARGET) $(OBJECTS)
