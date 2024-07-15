CC=g++
EXT=cpp

OPT=-O0
DBG=-g -gdwarf-2
WARNINGS=-Wall -Wextra -m64 -Wsign-conversion -Wconversion
DEPFLAGS=-MP -MD
DEF=-DTRACY_ENABLE

INCS=$(foreach DIR,$(INC_DIRS),-I$(DIR))
LIBS=$(foreach DIR,$(LIB_DIRS),-L$(DIR))
LIBS+=-lmingw32 -lSDL2main -lSDL2 -lSDL2_image -lSDL2_ttf -lws2_32 -ldbghelp

CFLAGS=$(DBG) $(OPT) $(INCS) $(LIBS) $(WARNINGS) $(DEPFLAGS) $(DEF)

INC_DIRS=. ./external/include/ ./include/
LIB_DIRS=. ./external/lib/
BUILD_DIR=build
CODE_DIRS=. src external/tracy-0.10/public
VPATH=$(CODE_DIRS)

SRC=$(foreach DIR,$(CODE_DIRS),$(wildcard $(DIR)/*.$(EXT)))
OBJ=$(addprefix $(BUILD_DIR)/,$(notdir $(SRC:.$(EXT)=.o)))
DEP=$(addprefix $(BUILD_DIR)/,$(notdir $(SRC:.$(EXT)=.d)))

PROJ=Main
EXEC=$(PROJ).exe

all: $(BUILD_DIR)/$(EXEC)
	@echo "========================================="
	@echo "              BUILD SUCCESS              "
	@echo "========================================="

$(BUILD_DIR)/%.o: %.cpp | $(BUILD_DIR)
	$(CC) -c  $< -o $@ $(CFLAGS)
$(BUILD_DIR)/$(EXEC): $(OBJ)
	$(CC)  $^ -o $@ $(CFLAGS)

$(BUILD_DIR):
	mkdir $@
	cp ./external/lib/*.dll ./build/
	$(info SRC_DIRS : $(CODE_DIRS))
	$(info INC_DIRS : $(INC_DIRS))
	$(info INCS     : $(INCS))
	$(info SRC_FILES: $(SRC))
	$(info OBJ_FILES: $(OBJ))
	@echo "========================================="

clean:
	rm -fR $(BUILD_DIR)

profile:
	start Tracy;start ./$(BUILD_DIR)/$(EXEC);

-include $(DEP)

.PHONY: all clean
