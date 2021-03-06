##############################################################################
# CMake file for building Femocs as a static library or standalone executable
#
##############################################################################

cmake_minimum_required (VERSION 2.8.8)

# ==============================================
# ======= Variables to be set on cmd-line ======
# ==============================================

## Set compilers
#SET(CMAKE_Fortran_COMPILER gfortran)
#SET(CMAKE_C_COMPILER gcc)
#SET(CMAKE_CXX_COMPILER c++)

# specify whether to build in debug or release mode
#SET(CMAKE_BUILD_TYPE Release)
#SET(CMAKE_BUILD_TYPE Debug)

# ==============================================
# ====== Set library name and verbosity =======
# ==============================================

MESSAGE(STATUS "Building " ${TARGET_TYPE} " in a " ${CMAKE_BUILD_TYPE} " mode")

# specify the name of the library
SET(TARGET "femocs")

# specify which languages project uses
PROJECT(${TARGET} C CXX Fortran)

# enable or disable verbose build
SET(CMAKE_VERBOSE_MAKEFILE off)

# set explicitly number of threads used by Deal.II
# if not set, determination will be done automatically
IF (N_DEALII GREATER 0)
    MESSAGE(STATUS "Setting \# Deal.II threads to " ${N_DEALII})
    SET(DEAL_II_NUM_THREADS N_DEALII)
ENDIF()

# ==============================================
# ========== Path for input & output ===========
# ==============================================

# Specify folders where femocs, libfemocs and its .mod-file will be written
# Those paths are (not like others) specified in relation to the location of Makefile
IF (TARGET_TYPE MATCHES lib)
    SET(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ../../lib)
    SET(CMAKE_Fortran_MODULE_DIRECTORY ../../lib)
ELSE()
    SET(CMAKE_RUNTIME_OUTPUT_DIRECTORY ..)
ENDIF()


# Specify the path to deal.II files
SET(DEAL_II_DIR ../dealii)

# Specify the path to CGAL files
SET(CGAL_DIR ../cgal)

# specify paths to tetgen, getelec and slatec libraries
SET(TET_DIR ../lib)
SET(GETELEC_DIR ../GETELEC/lib)
SET(SLATEC_DIR ../GETELEC/lib)

SET(TET_PATH ${CMAKE_SOURCE_DIR}/${TET_DIR}/libtet.a)
SET(GETELEC_PATH ${CMAKE_SOURCE_DIR}/${GETELEC_DIR}/libgetelec.a)
SET(SLATEC_PATH ${CMAKE_SOURCE_DIR}/${SLATEC_DIR}/libslatec.a)

# Find source files
IF (TARGET_TYPE MATCHES lib)
    FILE(GLOB TARGET_SRC "../src/*.cpp" "../src/*.f90")
ELSE()
    FILE(GLOB TARGET_SRC "../src/*.cpp")
ENDIF()

# Specify the path to main file separately
SET(TARGET_MAIN ../src/main/Main.cpp)

# path to the directories with headers
IF (LIBCGAL)
    INCLUDE_DIRECTORIES(../include ../GETELEC/modules ../cgal/include)
ELSE()
    INCLUDE_DIRECTORIES(../include ../GETELEC/modules)
ENDIF()

# ==============================================
# =========== Locate dependencies ==============
# ==============================================

FIND_PACKAGE(deal.II 8.4 REQUIRED HINTS ${DEAL_II_DIR})
IF ( NOT deal.II_FOUND )
    MESSAGE(FATAL_ERROR "Deal.II not found from " ${DEAL_II_DIR})
ENDIF()

IF (LIBCGAL)
    FIND_PACKAGE(CGAL HINTS ${CGAL_DIR})
    IF ( CGAL_FOUND )
        MESSAGE(STATUS "Using CGAL from " ${CGAL_DIR})
        INCLUDE( ${CGAL_USE_FILE} )
        SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DUSE_CGAL=true")
    ELSE()
        MESSAGE(FATAL_ERROR "CGAL not found from " ${CGAL_DIR}) 
    ENDIF()
ELSE()
    MESSAGE(STATUS "Disabling usage of CGAL")
ENDIF()


# ==============================================
# ============= Compilation flags ==============
# ==============================================

# in debug mode enable asserts
IF (CMAKE_BUILD_TYPE MATCHES Debug)
    MESSAGE(STATUS "Enabling FEMOCS asserts")
    SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DASSERTMODE=true")
ELSE()
    MESSAGE(STATUS "Disabling FEMOCS asserts")
ENDIF()

# add OpenMP flag
SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fopenmp")

# Specify warnings that are checked and that are omitted
#SET(WARNINGS "-pedantic -Wall -Wextra -Wno-unused-local-typedefs -Wno-unused-parameter")
SET(WARNINGS "")

# flags should not be set in CMAKE_CXX_FLAGS, because deal.II overrides many of them.
# The deal.II flags can be seen from dealii/share/deal.II/Make.global_options

# remove all flags that control warnings
STRING(REGEX REPLACE " -W[A-Za-z0-9-]+" "" DEAL_II_CXX_FLAGS ${DEAL_II_CXX_FLAGS})
STRING(REPLACE "-pedantic" "" DEAL_II_CXX_FLAGS ${DEAL_II_CXX_FLAGS})

# remove optimization level flags
STRING(REGEX REPLACE "-O[0-9]" "" DEAL_II_CXX_FLAGS_RELEASE ${DEAL_II_CXX_FLAGS_RELEASE})

# remove warning about flag not present for f90
STRING(REPLACE "-Wno-unused-local-typedefs" "" DEAL_II_CXX_FLAGS_RELEASE ${DEAL_II_CXX_FLAGS_RELEASE})

# set warning and optimization level
IF (CMAKE_BUILD_TYPE MATCHES Release)
    SET(DEAL_II_CXX_FLAGS "${DEAL_II_CXX_FLAGS} -O3 -w")
ELSE()
    SET(DEAL_II_CXX_FLAGS "${DEAL_II_CXX_FLAGS} -g -O0 ${WARNINGS}")
ENDIF()

# print out resulting flags
MESSAGE(STATUS "CMAKE_CXX_FLAGS = " ${CMAKE_CXX_FLAGS})
MESSAGE(STATUS "DEAL_II_CXX_FLAGS = " ${DEAL_II_CXX_FLAGS})
MESSAGE(STATUS "DEAL_II_CXX_FLAGS_RELEASE = " ${DEAL_II_CXX_FLAGS_RELEASE})
MESSAGE(STATUS "DEAL_II_LINKER_FLAGS = " ${DEAL_II_LINKER_FLAGS})
MESSAGE(STATUS "DEAL_II_LINKER_FLAGS_RELEASE = " ${DEAL_II_LINKER_FLAGS_RELEASE})
MESSAGE(STATUS "DEAL_II_USER_DEFINITIONS = " ${DEAL_II_USER_DEFINITIONS})
MESSAGE(STATUS "DEAL_II_USER_DEFINITIONS_RELEASE = " ${DEAL_II_USER_DEFINITIONS_RELEASE})

# ==============================================
# =========== Start building ===========
# ==============================================

DEAL_II_INITIALIZE_CACHED_VARIABLES()

IF (TARGET_TYPE MATCHES lib)
    ADD_LIBRARY (${TARGET} STATIC ${TARGET_SRC})
    DEAL_II_SETUP_TARGET (${TARGET})
ELSE()
    ADD_LIBRARY(tet ${TET_DIR})
    SET_TARGET_PROPERTIES(tet PROPERTIES LINKER_LANGUAGE CXX)

    ADD_LIBRARY(getelec ${GETELEC_DIR})
    SET_TARGET_PROPERTIES(getelec PROPERTIES LINKER_LANGUAGE Fortran)

    ADD_LIBRARY(slatec ${SLATEC_DIR})
    SET_TARGET_PROPERTIES(slatec PROPERTIES LINKER_LANGUAGE C)

    ADD_EXECUTABLE(${TARGET} ${TARGET_SRC} ${TARGET_MAIN})
    DEAL_II_SETUP_TARGET(${TARGET})
    TARGET_LINK_LIBRARIES(${TARGET} ${TET_PATH} ${GETELEC_PATH} ${SLATEC_PATH})
ENDIF()
