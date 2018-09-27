obj-$(CONFIG_HW_RANDOM_CS75XX) += cs75xx-rng.o

FDATE=`date "+%d%m%g"`
BDATE=`date "+%d%m"`
VERSION=1
RELEASE=3

ccflags-y += -DBDATE=0x$(BDATE)
ccflags-y += -DVERSION=$(VERSION)
ccflags-y += -DRELEASE=$(RELEASE) 

ccflags-y += -I$(obj)
ccflags-y += -I$(obj)/include

ccflags-y += -DRNG_MEMORY_BASE_OFFSET=0x0
ccflags-y += -DSDK_ENDIAN_LITTLE=1
ccflags-y += -DCPU_ENDIAN_LITTLE=1

cs75xx-rng-y += cs75xx-trng.o
cs75xx-rng-y += src/elptrng.o
cs75xx-rng-y += tools/trngdiag/cs75xx-trngdiag.o
