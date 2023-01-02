###############################################################
#https://www.gnu.org/software/make/manual/html_node/index.html
###############################################################

ifneq ($(INSIDEWRAPPER),1)
all:
%:
	@if [[ "$(MAKEFLAGS)" =~ -j ]]; then JOBS=""; else JOBS='-j8'; fi; \
	eval INSIDEWRAPPER=1 $(MAKE) $${JOBS} $(MAKECMDGOALS)
else

.PHONY: all lib
all:

# compiler setup
CC=g++
CFLAGS=-std=c++2a -ggdb $(addprefix -I ,$(libs)) -MMD -fuse-ld=gold -Wl,-no-allow-multiple-definition -Wall -Werror --pedantic -Wextra -lpthread -DIS_VM=$(IS_VM)
CFLAGS_DEBUG=-O0 -DDEBUG_MODE=1
CFLAGS_PROD=-O3

GTEST_LIBA=/usr/lib/x86_64-linux-gnu/libgtest.a
ifneq (,$(wildcard $(GTEST_LIBA)))
UTLIBS+=/usr/lib/x86_64-linux-gnu/libgtest.a 
else
CFLAGS+=-lgtest -march=native 
endif
DEPS=$(wildcard ./build/*/*.d)
-include $(DEPS)


modes=prod debug full
libs=utils
apps=app sandbox
tests=utest

define cflags 
	$(if $(filter debug,$1),$(CFLAGS_DEBUG),$(CFLAGS_PROD)) $(CFLAGS)
endef

define lib_rules
SRC_$1=$(wildcard $1/*.cpp)
OBJ_$1_$2=$$(addprefix build/$2/,$$(SRC_$1:.cpp=.o))
LIB_$1_$2=build/$2/$1/lib$1_$2.a
DUMMY_$1=$(shell mkdir -p build/$2/$1)	
LIB_ALL_$2+=$$(LIB_$1_$2)
all: $$(LIB_$1_$2)
lib: $$(LIB_$1_$2)
$$(LIB_$1_$2): $$(OBJ_$1_$2)
	ar -r $$@ $$^	
build/$2/$1/%.o: $1/%.cpp
	$(CC) $$< -c -o $$@ $(call cflags,$2)
endef

define ut_rules
SRC_$1=$(wildcard $1/*.cpp)
BIN_$1_$2=$$(addprefix build/,$$(addsuffix _$2,$$(SRC_$1:.cpp=)))
DUMMY_$1=$(shell mkdir -p build/$1)	
all: $$(BIN_$1_$2)
test: $$(BIN_$1_$2)
build/$1/%_$2: $1/%.cpp $$(LIB_ALL_$2)
	$(CC) -o $$@  $$< $(UTLIBS) $(call cflags,$2) -pthread $$(LIB_ALL_$2) && \
	echo "TESTING $$@" && rm -rf $$@.failed && \
  (./$$@ || mv $$@ $$@.failed)

endef

define app_rules
SRC_$1=$(wildcard $1/*.cpp)
BIN_$1_$2=$$(addprefix build/,$$(addsuffix _$2,$$(SRC_$1:.cpp=)))
DUMMY_$1=$(shell mkdir -p build/$1)
ifeq ($2,full)
WHOLE_ARCHIVE_START_$1_$2=-Wl,--whole-archive
WHOLE_ARCHIVE_END_$1_$2=-Wl,--no-whole-archive 
endif	
all: $$(BIN_$1_$2)
app: $$(BIN_$1_$2)	
build/$1/%_$2: $1/%.cpp $$(LIB_ALL_$2)
	$(CC) -o $$@  $$< $(call cflags,$2) $$(WHOLE_ARCHIVE_START_$1_$2) $$(LIB_ALL_$2) $$(WHOLE_ARCHIVE_END_$1_$2) 

endef

define mode_rules
$(foreach lib,$(libs),$(call lib_rules,$(lib),$1))
$(foreach test,$(tests),$(call ut_rules,$(test),$1))
$(foreach app,$(apps),$(call app_rules,$(app),$1))
endef

$(file >build/makefile.gen,$(foreach mode,$(modes),$(call mode_rules,$(mode))))
include build/makefile.gen


clean:
	@rm -rf build

endif	
