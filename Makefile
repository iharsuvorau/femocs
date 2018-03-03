#############################################################################
#     Makefile for building FEMOCS - Finite Elements on Crystal Surfaces
#                             Mihkel Veske 2016
#
#############################################################################
# Before running make taito, run
#   module load gcc/5.3.0 intelmpi/5.1.3
# Before running make alcyone, run
#   module load PrgEnv-gnu gcc/5.1.0

include build/makefile.defs

all: lib

lib: femocs_lib
femocs_lib:
	make -s -f build/makefile.lib lib/libfemocs.a

test_f90: femocs_lib femocs_f90
femocs_f90:
	make -f build/makefile.exec main=${FMAIN} cf=${FCFLAGS} compiler=${F90}

test_c: femocs_lib femocs_c
femocs_c:
	make -f build/makefile.exec main=${CMAIN} cf=${CCFLAGS} compiler=${CC}

solver: femocs_lib femocs_solver
femocs_solver:
	make -f build/makefile.exec main=${SOLVERMAIN} cf=${CXXCFLAGS} compiler=${CXX}

release: femocs_lib femocs_release
femocs_release:
	make -s -f build/makefile.exec build/femocs main=${CXXMAIN} cf=${CXXCFLAGS} compiler=${CXX}

debug: femocs_debug_lib femocs_debug
femocs_debug:
	make -s -f build/makefile.exec build/femocs.debug main=${CXXMAIN} cf=${CXXCFLAGS} compiler=${CXX}

femocs_debug_lib:
	make -s -f build/makefile.lib lib/libfemocs_debug.a

doc: femocs_doc
femocs_doc:
	make -f build/makefile.doc

ubuntu:
	mkdir -p share/.build && cd share/.build && rm -rf * && cmake .. -Dmode=ubuntu14
	make -s -f build/makefile.cgal release
	make -f build/makefile.install
	
ubuntu16:
	mkdir -p share/.build && cd share/.build && rm -rf * && cmake .. -Dmode=ubuntu16
	make -s -f build/makefile.cgal release
	make -f build/makefile.install	

taito:
	make -s -f build/makefile.cgal taito
	make -f build/makefile.install

alcyone:
	make -f build/makefile.install

clean:
	make -s -f build/makefile.lib clean
	make -s -f build/makefile.exec clean
	make -s -f build/makefile.doc clean

clean-all:
	rm -rf share/.build
	make -s -f build/makefile.lib clean-all
	make -s -f build/makefile.exec clean-all
	make -s -f build/makefile.install clean-all
	make -s -f build/makefile.cgal clean-all
	make -s -f build/makefile.doc clean-all

help:
	@echo ''
	@echo 'make all        pick default build type for Femocs'
	@echo 'make ubuntu     build in Ubuntu desktop all the external libraries that Femocs needs'
	@echo 'make ubuntu16   build in Ubuntu16 desktop all the external libraries that Femocs needs'
	@echo 'make taito      build in CSC Taito cluster all the external libraries that Femocs needs'
	@echo 'make alcyone    build in Alcyone cluster all the external libraries that Femocs needs'
	@echo 'make lib        build Femocs as static library'
	@echo 'make solver     build Femocs executable from main file in the FEM solver module'
	@echo 'make release    build Femocs executable from c++ main with highest optimization level'
	@echo 'make debug      build Femocs executable from c++ main with debugging features enabled'
	@echo 'make test_f90   build Femocs executable from Fortran main'
	@echo 'make test_c     build Femocs executable from C main'
	@echo 'make doc        generate Femocs documentation in html and pdf format
	@echo 'make clean      delete key files excluding installed libraries to start building from the scratch'
	@echo 'make clean-all  delete all the files and folders produced during the make process'
	@echo ''
