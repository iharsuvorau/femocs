/*
 * Femocs.h
 *
 *  Created on: 14.12.2015
 *      Author: veske
 */

#ifndef FEMOCS_H_
#define FEMOCS_H_

#include "LinearInterpolator.h"
#include "AtomReader.h"
#include "Config.h"
#include "Media.h"
#include "SolutionReader.h"
#include "physical_quantities.h"
#include "currents_and_heating.h"
#include "laplace.h"

using namespace std;
namespace femocs {

/** Main class to hold Femocs object */
class Femocs {
public:
    /**
     * Femocs constructor reads and stores configuration parameters
     * @param path_to_conf      path to the file holding the configuration parameters
     */
    Femocs(const string &path_to_conf);

    /** Femocs destructor */
    ~Femocs();

    /** Function to generate FEM mesh and to solve differential equation(s)
     * @param elfield   long range electric field strength
     * @param message   message from the host: time step, file name etc
     * @return          0 - function completed normally; 1 - function did not complete normally
     */
    int run(const double elfield, const string &message);

    /** Function to import atoms from PARCAS
     * @param n_atoms       number of imported atoms
     * @param coordinates   normalised coordinates of atoms in PARCAS format
     * @param box           size on simulation box in Angstroms
     * @param nborlist      neighbour list for atoms
     * @return              0 - import succeeded and current run differs from previous one, whole field calculation will be performed;
     *                      1 - import failed or current run is similar to the previous one, field calculation will be skipped
     */
    int import_atoms(const int n_atoms, const double* coordinates, const double* box, const int* nborlist);

    /** Function to import coordinates and types of atoms
     * @param n_atoms   number of imported atoms
     * @param x         x-coordinates of the atoms
     * @param y         y-coordinates of the atoms
     * @param z         z-coordinates of the atoms
     * @param types     types of the atoms
     * @return          0 - import succeeded and current run differs from previous one, whole field calculation will be performed;
     *                  1 - import failed or current run is similar to the previous one, field calculation will be skipped
     */
    int import_atoms(const int n_atoms, const double* x, const double* y, const double* z, const int* types);

    /** Function to import atoms from file
     * @param file_name path to the file
     * @return          0 - import succeeded, 1 - import failed
     */
    int import_atoms(const string& file_name);
    
    /** Function to export the calculated electric field on imported atom coordinates
     * @param n_atoms   number of points of interest
     * @param Ex        x-component of electric field
     * @param Ey        y-component of electric field
     * @param Ez        z-component of electric field
     * @param Enorm     norm of electric field
     */
    int export_elfield(const int n_atoms, double* Ex, double* Ey, double* Ez, double* Enorm);
    
    /** Function to export the calculated temperatures on imported atom coordinates
     * @param n_atoms   number of points of interest
     * @param T         temperature in the atom location
     */
    int export_temperature(const int n_atoms, double* T);

    /** Calculate and export charges & forces on imported atom coordinates
     * @param n_atoms   number of points of interest
     * @param xq        charges and forces in PARCAS format (xq[0] = q1, xq[1] = Fx1, xq[2] = Fy1, xq[3] = Fz1, xq[4] = q2, xq[5] = Fx2 etc)
     */
    int export_charge_and_force(const int n_atoms, double* xq);

    /** Function to linearly interpolate electric field at given points
     * @param n_points  number of points where electric field is interpolated
     * @param x         x-coordinates of the points of interest
     * @param y         y-coordinates of the points of interest
     * @param z         z-coordinates of the points of interest
     * @param Ex        x-component of the interpolated electric field
     * @param Ey        y-component of the interpolated electric field
     * @param Ez        z-component of the interpolated electric field
     * @param Enorm     norm of the interpolated electric field
     * @param flag      indicators showing the location of point; 0 - point was inside the mesh, 1 - point was outside the mesh
     * @return          0 - function used solution from current run; 1 - function used solution from previous run
     */
    int interpolate_elfield(const int n_points, const double* x, const double* y, const double* z,
            double* Ex, double* Ey, double* Ez, double* Enorm, int* flag);

    /** Function to linearly interpolate electric potential at given points
     * @param n_points  number of points where electric potential is interpolated
     * @param x         x-coordinates of the points of interest
     * @param y         y-coordinates of the points of interest
     * @param z         z-coordinates of the points of interest
     * @param phi       electric potential
     * @param flag      indicators showing the location of point; 0 - point was inside the mesh, 1 - point was outside the mesh
     * @return          0 - function used solution from current run; 1 - function used solution from previous run
     */
    int interpolate_phi(const int n_points, const double* x, const double* y, const double* z, double* phi, int* flag);

    /**
     * Function to parse integer argument of the command from input script
     * @param command   name of the command which's argument should be parsed
     * @param arg       parsed argument
     * @return          0 - command was found and arg was modified; 1 - command was not found and arg was not modified
     */
    int parse_command(const string& command, int* arg);

    /**
     * Function to parse double argument of the command from input script
     * @param command   name of the command which's argument should be parsed
     * @param arg       parsed argument
     * @return          0 - command was found and arg was modified; 1 - command was not found and arg was not modified
     */
    int parse_command(const string& command, double* arg);

private:
    bool skip_calculations, fail;
    double t0;

    AtomReader reader;      ///< all the imported atoms
    Config conf;            ///< configuration parameters
    Media dense_surf;       ///< non-coarsened surface atoms
    Media extended_surf;    ///< atoms added for the surface atoms

    TetgenMesh bulk_mesh;   ///< FEM mesh in bulk material
    TetgenMesh vacuum_mesh; ///< FEM mesh in vacuum
    LinearInterpolator bulk_interpolator;   ///< data for interpolating results in bulk
    LinearInterpolator vacuum_interpolator; ///< data for interpolating results in vacuum

    HeatReader temperatures = HeatReader(&bulk_interpolator);   ///< interpolated temperatures & current densities
    FieldReader fields = FieldReader(&vacuum_interpolator);     ///< interpolated fields and potentials
    ChargeReader face_charges = ChargeReader(&vacuum_interpolator); ///< charges on surface faces
    ForceReader forces = ForceReader(&vacuum_interpolator);     ///< forces on surface atoms

    fch::PhysicalQuantities phys_quantities;    ///< physical quantities used in heat calculations
    fch::CurrentsAndHeating<3> ch_solver1;      ///< first currents and heating solver
    fch::CurrentsAndHeating<3> ch_solver2;      ///< second currents and heating solver
    fch::CurrentsAndHeating<3>* ch_solver;      ///< active currents and heating solver
    fch::CurrentsAndHeating<3>* prev_ch_solver; ///< previous currents and heating solver
    
    /** Generate boundary nodes for mesh */
    int generate_boundary_nodes(Media& bulk, Media& coarse_surf, Media& vacuum);

    /** Generate bulk and vacuum meshes */
    int generate_meshes(TetgenMesh& bulk_mesh, TetgenMesh& vacuum_mesh);

    /** Solve Laplace equation */
    int solve_laplace(const TetgenMesh& mesh, fch::Laplace<3>& solver);

    /** Solve heat and continuity equations */
    int solve_heat(const TetgenMesh& mesh, fch::Laplace<3>& laplace_solver);
};

} /* namespace femocs */

#endif /* FEMOCS_H_ */
