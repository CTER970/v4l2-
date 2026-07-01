
CROSS_COMPILE = arm-linux-
AS		= $(CROSS_COMPILE)as
LD		= $(CROSS_COMPILE)ld
CC		= $(CROSS_COMPILE)gcc
CPP		= $(CC) -E
AR		= $(CROSS_COMPILE)ar
NM		= $(CROSS_COMPILE)nm

STRIP		= $(CROSS_COMPILE)strip
OBJCOPY		= $(CROSS_COMPILE)objcopy
OBJDUMP		= $(CROSS_COMPILE)objdump

export AS LD CC CPP AR NM
export STRIP OBJCOPY OBJDUMP

CFLAGS := -Wall -O2 -g
CFLAGS += -I $(shell pwd)/include
CFLAGS += -I $(shell pwd)/app_owned
CFLAGS += -I $(shell pwd)/app_owned/frame_queue
CFLAGS += -I $(shell pwd)/thread
CFLAGS += -pthread

LDFLAGS := -lm -ljpeg -lcurl -pthread

export CFLAGS LDFLAGS

TOPDIR := $(shell pwd)
export TOPDIR

TARGET := video2lcd
UPLOAD_TEST_TARGET := upload_test


obj-y += main.o
obj-y += app_owned/
obj-y += thread/
obj-y += capture/
obj-y += upload/
obj-y += convert/
obj-y += display/
obj-y += video/

all : start_recursive_build $(TARGET)
	@echo $(TARGET) has been built!

start_recursive_build:
	make -C ./ -f $(TOPDIR)/Makefile.build

$(TARGET) : start_recursive_build
	$(CC) -o $(TARGET) built-in.o $(LDFLAGS)

upload_test.o : upload_test.c
	$(CC) $(CFLAGS) -c -o $@ $<

$(UPLOAD_TEST_TARGET) : start_recursive_build upload_test.o
	$(CC) -o $(UPLOAD_TEST_TARGET) upload/upload.o upload_test.o $(LDFLAGS)

clean:
	rm -f $(shell find -name "*.o")
	rm -f $(TARGET)
	rm -f $(UPLOAD_TEST_TARGET)

distclean:
	rm -f $(shell find -name "*.o")
	rm -f $(shell find -name "*.d")
	rm -f $(TARGET)
	rm -f $(UPLOAD_TEST_TARGET)