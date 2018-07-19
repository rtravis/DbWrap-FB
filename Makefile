CC        := g++
LD        := g++
AR        := ar

CPPFLAGS  := -std=c++14 -O3 -Wall -fmessage-length=0 -fPIC
LDFLAGS   := -lfbclient -lpthread

MODULES   := fb test

SRC_DIR   := $(addprefix src/,$(MODULES))
BUILD_DIR := $(addprefix build/,$(MODULES))

SRC       := $(foreach sdir,$(SRC_DIR),$(wildcard $(sdir)/*.cpp))
OBJ       := $(patsubst src/%.cpp,build/%.o,$(SRC))
INCLUDES  := $(addprefix -I,$(SRC_DIR))

vpath %.cpp $(SRC_DIR)

define make-goal
$1/%.o: %.cpp
	$(CC) $(INCLUDES) $(CPPFLAGS) -c $$< -o $$@
endef

.PHONY: all checkdirs clean shared_lib static_lib unit_test

all: shared_lib static_lib unit_test

checkdirs: $(BUILD_DIR)

$(BUILD_DIR):
	@mkdir -p $@

shared_lib: checkdirs build/libDbWrap++FB.so

build/libDbWrap++FB.so: $(OBJ)
	$(LD) -shared -Wl,-soname,libDbWrap++FB.so $^ -o $@ $(LDFLAGS)

static_lib: checkdirs build/libDbWrap++FB.a

build/libDbWrap++FB.a: $(OBJ)
	$(AR) cr $@ $^

unit_test: build/DbWrap++FBUnitTest

build/DbWrap++FBUnitTest: shared_lib
	$(LD) build/test/FbDbUnitTest.o -lDbWrap++FB -Lbuild -Wl,-rpath,\$$ORIGIN -o $@ $(LDFLAGS)

clean:
	@rm -rf $(BUILD_DIR) build/libDbWrap++FB.so \
						build/libDbWrap++FB.a \
						build/DbWrap++FBUnitTest \
						build/DbWrap++FBUnitTest_st

$(foreach bdir,$(BUILD_DIR),$(eval $(call make-goal,$(bdir))))
