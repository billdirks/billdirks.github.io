SRCDIR := lib
BUILDDIR := build
HEADERDIR := include

# All source files
SOURCES := $(shell find $(SRCDIR) -name '*.cpp')

# All object files
OBJECTS := $(addprefix $(BUILDDIR)/,$(SOURCES:%.cpp=%.o))

CXX := g++
CXXFLAGS := -Wall --std=c++20 -I$(HEADERDIR)

.PHONEY: clean
clean:
	rm -fr $(BUILDDIR)/*

# Build object files
$(BUILDDIR)/%.o: %.cpp
	mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -c $< -o $@

# build library
$(BUILDDIR)/libmetricplayground.a: $(OBJECTS)
	ar rvs $(BUILDDIR)/libmetricplayground.a $(OBJECTS)

.PHONEY: libmetricplayground.a
libmetricplayground.a: $(BUILDDIR)/libmetricplayground.a

# Build compute
$(BUILDDIR)/compute: examples/compute.cpp libmetricplayground.a
	$(CXX) $(CXXFLAGS) examples/compute.cpp  -lmetricplayground -L$(BUILDDIR) -o $@
.PHONEY: compute
compute: $(BUILDDIR)/compute

# Build compute_opt
$(BUILDDIR)/compute_opt: CXXFLAGS += -O3
$(BUILDDIR)/compute_opt: $(BUILDDIR)/compute
.PHONEY: compute_opt
compute_opt: $(BUILDDIR)/compute_opt

# IT DOESN'T INCREMENTALLY BUILD AND IS A NOTE TO MYSELF
$(BUILDDIR)/compute_without_lib: $(SOURCES) examples/compute.cpp
	$(CXX) $(CXXFLAGS) $(SOURCES) examples/compute.cpp -o build/compute_without_lib

$(BUILDDIR)/compute_without_lib_opt: CXXFLAGS += -O3
$(BUILDDIR)/compute_without_lib_opt: $(BUILDDIR)/compute_without_lib
.PHONEY: compute_without_lib_opt
compute_without_lib_opt: $(BUILDDIR)/compute_without_lib_opt

# Utils
random_number: CXXFLAGS += -O3
random_number:
	$(CXX) $(CXXFLAGS) utils/random_number.cpp -o random_number

# TODO: Add debug/profiling build targets
