# $(ROOT)/mpp/code/component/pci/pciv/Makefile

#ifeq ($(PARAM_FILE), ) 
#    PARAM_FILE:=../../../Makefile.param
#    include $(PARAM_FILE)
#endif


PWD_PATH := $(shell pwd)

INC_PATH := -I$(PWD_PATH)/../include

export ARCH=arm

#export CROSS_COMPILE?=arm-hisiv300-linux-
#export CROSS?=arm-hisiv300-linux-
export CROSS=

export CC:=$(CROSS)gcc
export AR:=$(CROSS)ar

export MPP_CFLAGS:= -Wall



ifeq ($(HIGDB),HI_GDB)
FLAGS += -Wall -c -g
else
FLAGS += -Wall -c -O2
endif

FLAGS += -I. $(INC_PATH)

#pciv compile(cd $(PWD)/kernel; make) will use FLAGS and LINUXROOT
export FLAGS
export LINUX_ROOT

all:$(MPP_OBJ)
	@$(CC) -o mpi_pciv.o  mpi_pciv.c $(FLAGS) -fPIC
	@$(CC) -o pciv_msg.o pciv_msg.c  $(FLAGS) -fPIC
	@$(CC) -fPIC -shared -o libpciv.so mpi_pciv.o
	@$(AR) -rsv libpciv.a mpi_pciv.o pciv_msg.o
	cp libpciv.a ../../bin/
clean:
	rm -f mpi_pciv.o libpciv.a $(REL_LIB)/libpciv.a libpciv.so $(REL_LIB)/libpciv.so $(REL_INC)/mpi_pciv.h
aa
