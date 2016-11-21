#############################################################################
#     Makefile for building FEMOCS - Finite Elements on Crystal Surfaces
#                             Mihkel Veske 2016
#
#############################################################################

include release/makefile.defs

all: lib

lib: lib/libfemocs.a 
lib/libfemocs.a: src/* include/* 
	make -f release/makefile.lib
		
release: femocs.release
femocs.release: ${MAIN_CPP} src/* include/* 
	make -f release/makefile.release mode=Release
	
debug: femocs.debug
femocs.debug: ${MAIN_CPP} src/* include/* 
	make -f release/makefile.release mode=Debug
	
fortran: femocs.fortran
femocs.fortran: ${MAIN_F90} src/* include/*
	make -f release/makefile.fortran
	
install:
	make -f release/makefile.install
	
clean:
	make -s -f release/makefile.lib clean
	make -s -f release/makefile.release clean
	make -s -f release/makefile.fortran clean

clean-all:
	make -s -f release/makefile.lib clean-all
	make -s -f release/makefile.release clean-all
	make -s -f release/makefile.fortran clean-all
	make -s -f release/makefile.install clean-all

help:
	@echo ''
	@echo 'make all        pick default build type for Femocs'
	@echo 'make lib        build Femocs as static library'
	@echo 'make release    build Femocs executable from c++ main with highest optimization level'
	@echo 'make debug      build Femocs executable from c++ main with debugging features enabled'
	@echo 'make fortran    build Femocs executable from Fortran main in release mode'
	@echo 'make install    build all the external libraries that Femocs needs'
	@echo 'make clean      delete key files excluding installed libraries to start building from the scratch'
	@echo 'make clean-all  delete all the files and folders produced during make process'
	@echo ''
