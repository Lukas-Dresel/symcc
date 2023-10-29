.PHONY: all clean


all: $(BUILD_DIR) $(BUILD_DIR)/libc_harness.so $(BUILD_DIR)/libc_harness.a

$(BUILD_DIR):
	mkdir -p $@

$(BUILD_DIR)/bsearch.o: bsearch.c
	$(CC) $(CFLAGS) -c -o $@ $<

$(BUILD_DIR)/qsort.o: qsort.c
	$(CC) $(CFLAGS) -c -o $@ $<

$(BUILD_DIR)/libc_harness.so: $(BUILD_DIR)/bsearch.o $(BUILD_DIR)/qsort.o
	$(CC) $(CFLAGS) -shared -o $@ $^

$(BUILD_DIR)/libc_harness.a: $(BUILD_DIR)/bsearch.o $(BUILD_DIR)/qsort.o
	ar rcs $@ $^

clean:
	rm -f $(BUILD_DIR)/*.{o,so,a}
