/*
 * Femocs.h
 *
 *  Created on: 14.12.2015
 *      Author: veske
 */

#ifndef FEMOCS_H_
#define FEMOCS_H_

#include "Macros.h"

using namespace std;

/**
 * Main class to hold Femocs object
 */
class Femocs {
public:
    /**
     * Constructor of Femocs reads in and stores input parameters.
     * @param file_name - path to Femocs input script.
     */
    Femocs(string file_name);

    /** Struct holding data about input parameters. */
    struct Config {
        string mesher;          //!< simulation cell finite element mesher
        string infile;          //!< path to input script
        double latconst;        //!< lattice constant
        double coord_cutoff;    //!< cutoff distance in Angstroms for Coordination analysis
        int nnn;                //!< number of nearest neighbours for given crystal structure
        int nt;                 //!< number of OpenMP threads
        int poly_degree;     //!< polynomial degree of the finite element (1-linear, 2-quadratic,..)
        double neumann;         //!< value of Neumann boundary condition
    };

    Config conf;          //!< Femocs configuration parameters

    /**
     * The function to get input data, to start the Femocs simulation and
     * to return simulation results to the caller.
     * @param E0 - long range electric field
     * @param BC - boundary conditions
     * @param phi_guess - guess values of field potential for FEM solver
     * @param grid_spacing - FDM grid spacing in x, y and z direction
     */
//    const void run_femocs(const double E0, double*** BC, double*** phi_guess,
//            const double* grid_spacing);
    const void run(double c);

private:
};

const void femocs_speaker(string path);

#endif /* FEMOCS_H_ */