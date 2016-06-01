include ../Makefile.config

ARFLAGS += -rs

DEPS    := $(wildcard *.h)
DEPS    += $(wildcard *.hpp)
SRCS     = $(wildcard *.cpp)
OBJS     = $(SRCS:.cpp=.o)
MAIN     = dem.a

OBJECTS  = $(addprefix $(OBJDIR)/, $(OBJS))
TARGET   = $(OBJDIR)/$(MAIN)

.PHONY: all
all: $(TARGET)

$(TARGET): $(OBJDIR) $(OBJECTS)
	$(AR) $(ARFLAGS) $(TARGET) $(OBJECTS)

$(OBJDIR):
	$(MKDIR) $(OBJDIR)

$(OBJDIR)/%.o: %.cpp $(DEPS)
	$(CXX) $(CFLAGS) $(INCLUDE) -c $< -o $@ $(LDFLAGS) $(LDLIBS)

.PHONY: clean
clean:
	$(RM) $(TARGET) $(OBJECTS)
