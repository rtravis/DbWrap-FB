CC        := g++
LD        := g++
AR        := ar

CPPFLAGS  := -std=c++0x -O3 -Wall -fmessage-length=0 -fPIC
LDFLAGS   := -lfbembed -lpthread

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

.PHONY: all checkdirs clean

all: checkdirs static_lib shared_lib build/DbWrap++FBUnitTest


build/DbWrap++FBUnitTest: $(OBJ)
	$(LD) $^ -o $@ $(LDFLAGS)

static_lib: build/libDbWrap++FB.a

build/libDbWrap++FB.a: $(OBJ)
	$(AR) cr $@ $^


shared_lib: build/libDbWrap++FB.so


build/libDbWrap++FB.so: $(OBJ)
	$(LD) -shared -Wl,-soname,libDbWrap++FB.so $^ -o $@ $(LDFLAGS)

checkdirs: $(BUILD_DIR)

$(BUILD_DIR):
	@mkdir -p $@

clean:
	@rm -rf $(BUILD_DIR) build/DbWrap++FBUnitTest build/libDbWrap++FB.a build/libDbWrap++FB.so


$(foreach bdir,$(BUILD_DIR),$(eval $(call make-goal,$(bdir))))
