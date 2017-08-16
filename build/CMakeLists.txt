#############################################################################
#           CMake file for building Femocs as a static library
#            The file is supposed to be called by makefile.lib
#############################################################################

cmake_minimum_required (VERSION 2.8.8)

# specify the name of the library
SET(LIB_NAME "femocs")

# specify which languages project uses
PROJECT(${LIB_NAME} C CXX Fortran)

# enable or disable verbose build
SET(CMAKE_VERBOSE_MAKEFILE off)

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
# ========== Path for input & output ===========
# ==============================================

# Specify folders where libfemocs and its .mod-file will be written
# Those paths are (not like others) specified in relation to the location of Makefile
SET(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ../../lib)
SET(CMAKE_Fortran_MODULE_DIRECTORY ../../lib)

# Specify the path to deal.II files
SET(DEAL_II_DIR ../dealii)

# Specify the path to CGAL files
SET(CGAL_DIR ../cgal)

# Find source files
FILE(GLOB_RECURSE TARGET_SRC "../src/*.cpp" "../src/*.f90" "../deal-solver/source/*")

# path to the directories with headers
IF (LIBCGAL AND CMAKE_BUILD_TYPE MATCHES Release)
    INCLUDE_DIRECTORIES(../include ../deal-solver/include ../cgal/include ../GETELEC/modules)
ELSE()
    INCLUDE_DIRECTORIES(../include ../deal-solver/include ../GETELEC/modules)
ENDIF()

# ==============================================
# =========== Locate dependencies ==============
# ==============================================

FIND_PACKAGE(deal.II 8.4 REQUIRED HINTS ${DEAL_II_DIR})
IF ( NOT deal.II_FOUND )
    MESSAGE(FATAL_ERROR "Deal.II not found from " ${DEAL_II_DIR})
ENDIF()

IF (LIBCGAL AND CMAKE_BUILD_TYPE MATCHES Release)
    FIND_PACKAGE( CGAL HINTS ${CGAL_DIR} )
    IF (CGAL_FOUND)
        MESSAGE(STATUS "Using CGAL from " ${CGAL_DIR})
        INCLUDE( ${CGAL_USE_FILE} )
        SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DUSE_CGAL=true")
    ELSE()
        MESSAGE(FATAL_ERROR "CGAL not found from " ${CGAL_DIR}) 
    ENDIF()
ELSE()
    MESSAGE(STATUS "Not using CGAL!")
ENDIF()

FIND_PACKAGE( Boost REQUIRED )
IF ( NOT Boost_FOUND )
    MESSAGE(FATAL_ERROR "Boost not found") 
ENDIF()
    
FIND_PACKAGE(OpenMP)
IF ( NOT OPENMP_FOUND )
    MESSAGE(FATAL_ERROR "OpenMP not found") 
ENDIF()

# ==============================================
# ============= Compilation flags ==============
# ==============================================

#add OpenMP flag
SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${OpenMP_CXX_FLAGS}")

# Specify warnings that are checked and that are omitted
SET(WARNINGS "-pedantic -Wall -Wextra -Wno-unused-local-typedefs -Wno-unused-parameter")

# flags should not be set in CMAKE_CXX_FLAGS, because deal.II overrides many of them.
# The deal.II flags can be seen from dealii/share/deal.II/Make.global_options

# remove all flags that control warnings
STRING(REGEX REPLACE " -W[A-Za-z0-9-]+" "" DEAL_II_CXX_FLAGS ${DEAL_II_CXX_FLAGS})
STRING(REPLACE "-pedantic" "" DEAL_II_CXX_FLAGS ${DEAL_II_CXX_FLAGS})

# remove optimization level flags
STRING(REGEX REPLACE "-O[0-9]" "" DEAL_II_CXX_FLAGS_RELEASE ${DEAL_II_CXX_FLAGS_RELEASE})

# set warning and optimization level
IF (CMAKE_BUILD_TYPE MATCHES Release)
    SET(DEAL_II_CXX_FLAGS "${DEAL_II_CXX_FLAGS} -O3 -w")
ELSE()    
    SET(DEAL_II_CXX_FLAGS "${DEAL_II_CXX_FLAGS} -g -O1 ${WARNINGS}")
ENDIF()

# ==============================================
# =========== Start building library ===========
# ==============================================

DEAL_II_INITIALIZE_CACHED_VARIABLES()
ADD_LIBRARY (${LIB_NAME} STATIC ${TARGET_SRC})
DEAL_II_SETUP_TARGET (${LIB_NAME})