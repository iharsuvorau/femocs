#!/bin/bash

source ./build/makefile.defs

mode=$1

if (test $mode = taito) then
    echo "Loading Taito modules"
    
    source $MODULESHOME/init/bash
    module load gcc/5.3.0 intelmpi/5.1.3

    echo -e "\nExporting TBB path"
    LD_LIBRARY_PATH=${LIBPATH_TAITO:2}:${LD_LIBRARY_PATH}
    export LD_LIBRARY_PATH
fi

if (test $mode = alcyone) then
    echo "Loading Alcyone modules"

    source $MODULESHOME/init/bash
    module load PrgEnv-gnu gcc/5.1.0
fi

