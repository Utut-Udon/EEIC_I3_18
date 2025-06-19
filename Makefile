CC = gcc
TARGET = i1i2i3_phone
SRC = i1i2i3_phone.c

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) -o $(TARGET) $(SRC)

clean:
	rm -f $(TARGET)
re: clean all
