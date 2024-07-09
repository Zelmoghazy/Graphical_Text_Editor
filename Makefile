CC=gcc

OPT=-O0
DBG=-g -gdwarf-2
WARNINGS=-Wall -Wextra -m64 -Wsign-conversion -Wconversion
DEPFLAGS=-MP -MD

INCS=$(foreach DIR,$(INC_DIRS),-I$(DIR))
LIBS=$(foreach DIR,$(LIB_DIRS),-L$(DIR))
LIBS+=-lmingw32 -lSDL2main -lSDL2 -lSDL2_image -lSDL2_ttf

CFLAGS=$(DBG) $(OPT) $(INCS) $(LIBS) $(WARNINGS) $(DEPFLAGS)

INC_DIRS=. ./external/include/ ./include/
LIB_DIRS=. ./external/lib/
BUILD_DIR=build
CODE_DIRS=. external src
VPATH=$(CODE_DIRS)

SRC=$(foreach DIR,$(CODE_DIRS),$(wildcard $(DIR)/*.c))

OBJ=$(addprefix $(BUILD_DIR)/,$(notdir $(SRC:.c=.o)))
DEP=$(addprefix $(BUILD_DIR)/,$(notdir $(SRC:.c=.d)))


EXEC=Main.exe

all: $(BUILD_DIR)/$(EXEC)
	@echo "========================================="
	@echo "              BUILD SUCCESS              "
	@echo "========================================="

$(BUILD_DIR)/%.o: %.c | $(BUILD_DIR)
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

-include $(DEP)

.PHONY: all clean