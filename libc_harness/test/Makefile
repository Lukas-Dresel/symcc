.PHONY: all

all: test

test: test.c
	SYMCC_RUNTIME_DIR=../../libfuzzer_stb_image_symcts/runtime/target/release $(CC) $(CFLAGS) -o $@ $<

clean:
	rm -f test