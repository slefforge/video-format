CC      := cc
CFLAGS  := -O2 -Wall -Wextra

BIN_V1  := bin/v1
SRC_V1  := src/v1
ENC_V1  := $(BIN_V1)/myenc
DEC_V1  := $(BIN_V1)/mydec

.PHONY: all clean

all: $(ENC_V1) $(DEC_V1)

$(BIN_V1):
	mkdir -p $(BIN_V1)

$(ENC_V1): $(SRC_V1)/myenc.c | $(BIN_V1)
	$(CC) $(CFLAGS) $< -o $@

$(DEC_V1): $(SRC_V1)/mydec.c | $(BIN_V1)
	$(CC) $(CFLAGS) $< -o $@

clean:
	rm -rf bin out
