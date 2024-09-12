# Makefile

# Compiler settings - Can be customized.
CXX = g++
CXXFLAGS = -Wall -std=c++11 -mavx512f -mavx512bw -O3 -g
LDFLAGS = -pthread -O3 -g

# Makefile settings - Can be customized.
APPNAME = extract_app
SRCDIR = .
BUILDDIR = ./build

# Add all your .cpp files here (source files, excluding header files)
SOURCES = $(SRCDIR)/main.cpp $(SRCDIR)/extractor.cpp
OBJECTS = $(SOURCES:$(SRCDIR)/%.cpp=$(BUILDDIR)/%.o)

# Targets that aren't filenames
.PHONY: all clean directories

# Standard build
all: directories $(BUILDDIR)/$(APPNAME)

# Rule for creating the build directory
directories:
	@mkdir -p $(BUILDDIR)

# Linking the executable from the object files
$(BUILDDIR)/$(APPNAME): $(OBJECTS)
	$(CXX) $(LDFLAGS) -o $@ $^

# Compiling the object files from source files
$(BUILDDIR)/%.o: $(SRCDIR)/%.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

# Cleaning everything including the executable and object files
clean:
	$(RM) -r $(BUILDDIR)
