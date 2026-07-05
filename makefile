CC = gcc
CFLAGS = -Wall -Wextra -g -pthread
SRC = src/kv_store.c src/lru_list.c src/main.c
TARGET = kvstore

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) -o $(TARGET) $(SRC)

tsan: $(SRC)
	$(CC) $(CFLAGS) -fsanitize=thread -o kvstore_tsan $(SRC)

asan: $(SRC)
	$(CC) $(CFLAGS) -fsanitize=address -o kvstore_asan $(SRC)

clean:
	rm -f $(TARGET) kvstore_tsan kvstore_asan

.PHONY: clean tsan asan