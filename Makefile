## ************************************************************************
##  Makefile for comp_inj that will be used to test injections into
##  the output of the compressed data of SZ and ZFP.

##	Makefile for comp_inj that will be used to test injections into the 
##	output of the compressed data of SZ and ZFP via Libpressio.

##  PLEASE UPDATE THESE VARIABLES BEFORE COMPILING

##  COMPILER 
CC	= gcc

## SZ Random Access Flag
#SZ_RA = true
SZ_RA = false

## Libpressio flags
LIBPRESSIO_INCLUDE = /home/dakotaf/git/spack/opt/spack/linux-centos8-sandybridge/gcc-8.3.1/libpressio-0.42.2-kj7vnnwjc73rxktfiwguvmmtiuzzdmez
LIBPRESSIO_SO_PATH = /home/dakotaf/git/spack/opt/spack/linux-centos8-sandybridge/gcc-8.3.1/libpressio-0.42.2-kj7vnnwjc73rxktfiwguvmmtiuzzdmez/lib64

LIBPRESSIO_SZ_RA_INCLUDE = /home/dakotaf/git/spack/opt/spack/linux-centos8-sandybridge/gcc-8.3.1/libpressio-0.42.2-52tna67lljtmr5iaezfhzmynse2xqhsq
LIBPRESSIO_SZ_RA_SO_PATH = /home/dakotaf/git/spack/opt/spack/linux-centos8-sandybridge/gcc-8.3.1/libpressio-0.42.2-52tna67lljtmr5iaezfhzmynse2xqhsq/lib64

## SZ flags
SZ_INCLUDE = /home/dakotaf/git/spack/opt/spack/linux-centos8-sandybridge/gcc-8.3.1/sz-2.1.8.1-7dwx52g2namwrla5wjle5dkdeghojuxm
SZ_SO_PATH = /home/dakotaf/git/spack/opt/spack/linux-centos8-sandybridge/gcc-8.3.1/sz-2.1.8.1-7dwx52g2namwrla5wjle5dkdeghojuxm/lib64

SZ_RA_INCLUDE = /home/dakotaf/git/spack/opt/spack/linux-centos8-sandybridge/gcc-8.3.1/sz-2.1.8.1-3np35xve4f3z2wtwdjskzp7vqll54wa6
SZ_RA_SO_PATH = /home/dakotaf/git/spack/opt/spack/linux-centos8-sandybridge/gcc-8.3.1/sz-2.1.8.1-3np35xve4f3z2wtwdjskzp7vqll54wa6/lib64

## ZFP flags
ZFP_INCLUDE = /home/dakotaf/git/spack/opt/spack/linux-centos8-sandybridge/gcc-8.3.1/zfp-0.5.5-5w4tdpnbldp2xb3azjpei3o435e2u2jj
ZFP_SO_PATH = /home/dakotaf/git/spack/opt/spack/linux-centos8-sandybridge/gcc-8.3.1/zfp-0.5.5-5w4tdpnbldp2xb3azjpei3o435e2u2jj/lib64

## Compilation Includes
FLAGS = -I $(LIBPRESSIO_INCLUDE)/include/libpressio -I $(SZ_INCLUDE)/include/sz -I $(ZFP_INCLUDE)/include -L $(LIBPRESSIO_SO_PATH) -L $(SZ_SO_PATH) -L $(ZFP_SO_PATH) -llibpressio -lSZ -lzfp -lm
FLAGS_SZ_RA = -I $(LIBPRESSIO_SZ_RA_INCLUDE)/include/libpressio -I $(SZ_RA_INCLUDE)/include/sz -I $(ZFP_INCLUDE)/include -L $(LIBPRESSIO_SZ_RA_SO_PATH) -L $(SZ_RA_SO_PATH) -L $(ZFP_SO_PATH) -llibpressio -lSZ -lzfp -lm


## TARGETS
all: comp_inj comp_inj_w_output libpressio_example_sz libpressio_example_zfp

comp_inj:	comp_inj.c
ifeq ($(SZ_RA),true)
	$(CC) -Wall -g -rdynamic -o comp_inj comp_inj.c $(FLAGS_SZ_RA)
else 
	$(CC) -Wall -g -rdynamic -o comp_inj comp_inj.c $(FLAGS)
endif

comp_inj_w_output:	comp_inj_w_output.c
ifeq ($(SZ_RA),true)
	$(CC) -Wall -g -rdynamic -o comp_inj_w_output comp_inj_w_output.c $(FLAGS_SZ_RA)
else 
	$(CC) -Wall -g -rdynamic -o comp_inj_w_output comp_inj_w_output.c $(FLAGS)
endif

libpressio_example_sz:	libpressio_example_sz.c
ifeq ($(SZ_RA),true)
	$(CC) -Wall -g -rdynamic -o libpressio_example_sz libpressio_example_sz.c $(FLAGS_SZ_RA)
else 
	$(CC) -Wall -g -rdynamic -o libpressio_example_sz libpressio_example_sz.c $(FLAGS)
endif

libpressio_example_zfp:	libpressio_example_zfp.c
ifeq ($(SZ_RA),true)
	$(CC) -Wall -g -rdynamic -o libpressio_example_zfp libpressio_example_zfp.c $(FLAGS_SZ_RA)
else
	$(CC) -Wall -g -rdynamic -o libpressio_example_zfp libpressio_example_zfp.c $(FLAGS)
endif

clean:
	rm comp_inj
	rm comp_inj_w_output
	rm libpressio_example_sz
	rm libpressio_example_zfp

