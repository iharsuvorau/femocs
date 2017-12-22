/*
 * SolutionReader.h
 *
 *  Created on: 06.06.2016
 *      Author: veske
 */

#ifndef SOLUTIONREADER_H_
#define SOLUTIONREADER_H_

#include "Primitives.h"
#include "Medium.h"
#include "TetgenCells.h"
#include "TetgenMesh.h"
#include "VoronoiMesh.h"
#include "currents_and_heating.h"
#include "currents_and_heating_stationary.h"
#include "LinearInterpolator.h"

using namespace std;
namespace femocs {

/** Class to interpolate solution from DealII calculations */
class SolutionReader: public Medium {
public:
    /** SolutionReader constructors */
    SolutionReader();
    SolutionReader(TriangleInterpolator* tri, TetrahedronInterpolator* tet,
            const string& vec_lab, const string& vec_norm_lab, const string& scal_lab);

    /** Interpolate solution on the system atoms using tetrahedral interpolator
     * @param component component of result to interpolate: -1-locate atoms, 0-vector and scalar data, 1-vector data, 2-scalar data
     * @param srt       sort atoms spatially */
    void calc_3d_interpolation(const int component, const bool srt);

    /** Interpolate solution on the system atoms using triangular interpolator */
    void calc_2d_interpolation(const int component, const bool srt);
    void calc_2d_interpolation(vector<int>& atom2face, const int component, const bool srt);

    /** Reserve memory for data */
    void reserve(const int n_nodes);

    /** Print statistics about interpolated solution */
    void print_statistics();

    /** Append solution */
    void append_interpolation(const Solution& s);

    /** Get pointer to interpolation vector */
    vector<Solution>* get_interpolations();

    /** Get i-th Solution */
    Solution get_interpolation(const int i) const;

    /** Set i-th Solution */
    void set_interpolation(const int i, const Solution& s);

    /** Calculate statistics about coordinates and solution */
    void calc_statistics();

    /** Check and replace the NaNs and peaks in the solution */
    bool clean(const double r_cut, const bool use_hist_clean);

    /** Statistics about solution */
    struct Statistics {
        double vec_norm_min;  ///< minimum value of vector norm
        double vec_norm_max;  ///< maximum value of vector norm
        double scal_min;      ///< minimum value of scalar
        double scal_max;      ///< maximum value of scalar
    } stat;

protected:
    const string vec_label;       ///< label for vector data
    const string vec_norm_label;  ///< label for data associated with vector length
    const string scalar_label;    ///< label for scalar data
    double limit_min;             ///< minimum value of accepted comparison value
    double limit_max;             ///< maximum value of accepted comparison value

    TriangleInterpolator* interpolator_2d;    ///< data needed for interpolating on surface
    TetrahedronInterpolator* interpolator_3d; ///< data needed for interpolating in space
    vector<Solution> interpolation;           ///< interpolated data

    /** Initialise statistics about coordinates and solution */
    void init_statistics();

    /** Get i-th entry from all data vectors for .xyz file;
     * i < 0 gives the header of data vectors */
    string get_data_string(const int i) const;

    /** Get information about data vectors for .vtk file. */
    void get_point_data(ofstream& out) const;

    /** Function to clean the result from peaks
     * The cleaner makes the histogram for given component of electric field and applies smoothing
     * for the atoms, where field has diverging values.
     * For example, if histogram looks like [10 7 2 4 1 4 2 0 0 2], then the field on the two atoms that
     * made up the entries to last bin will be replaced by the average field around those two atoms. */
    void histogram_clean(const int coordinate, const double r_cut);

    void get_histogram(vector<int> &bins, vector<double> &bounds, const int coordinate);

    Solution get_average_solution(const int I, const double r_cut);

    int get_nanotip(Medium& nanotip, vector<bool>& atom_in_nanotip, const double radius);
};

/** Class to extract solution from DealII calculations */
class FieldReader: public SolutionReader {
public:
    /** FieldReader constructors */
    FieldReader(TriangleInterpolator* ip);
    FieldReader(TetrahedronInterpolator* ip);
    FieldReader(TriangleInterpolator* tri, TetrahedronInterpolator* tet);

    /** Interpolate solution on medium atoms using the solution on tetrahedral mesh nodes */
    void interpolate(const Medium &medium, const int component, const bool srt);

    /** Interpolate solution on points using the solution on tetrahedral mesh nodes */
    void interpolate(const int n_points, const double* x, const double* y, const double* z,
            const int component, const bool srt);

    /** Interpolate solution on medium atoms using the solution on triangular mesh nodes */
    void interpolate_2d(vector<int>& surf2face, const Medium &medium, const int component, const bool srt);

    /** Interpolate solution on points using the solution on triangular mesh nodes */
    void interpolate_2d(const int n_points, const double* x, const double* y, const double* z,
            const int component, const bool srt);

    /** Calculate the electric field for the stationary current and temperature solver */
    void transfer_elfield(fch::CurrentsAndHeatingStationary<3>* ch_solver, const double r_cut, const double use_hist_clean);

    /** Calculate the electric field for the transient current and temperature solver */
    void transfer_elfield(fch::CurrentsAndHeating<3>& ch_solver, const double r_cut, const double use_hist_clean);

    /** Interpolate electric field on set of points using the solution on tetrahedral mesh nodes
     * @return  index of first point outside the mesh; index == -1 means all the points were inside the mesh */
    void export_elfield(const int n_points, double* Ex, double* Ey, double* Ez, double* Enorm, int* flag);

    /** Interpolate electric potential on set of points using the solution on tetrahedral mesh nodes
     * @return  index of first point outside the mesh; index == -1 means all the points were inside the mesh */
    void export_potential(const int n_points, double* phi, int* flag);

    /** Export calculated electic field distribution to HOLMOD */
    void export_solution(const int n_atoms, double* Ex, double* Ey, double* Ez, double* Enorm);

    /** Return electric field in i-th interpolation point */
    Vec3 get_elfield(const int i) const;

    /** Return electric field norm in i-th interpolation point */
    double get_elfield_norm(const int i) const;

    /** Return electric potential in i-th interpolation point */
    double get_potential(const int i) const;

    /** Compare the analytical and calculated field enhancement.
     * The check is disabled if lower and upper limits are the same. */
    bool check_limits(const vector<Solution>* solutions=NULL) const;

    /** Set parameters to calculate analytical solution */
    void set_check_params(const double E0, const double limit_min, const double limit_max,
            const double radius1, const double radius2=-1);

private:
    /** Data needed for comparing numerical solution with analytical one */
    double E0;                      ///< Long-range electric field strength
    double radius1;                 ///< Minor semi-axis of ellipse
    double radius2;                 ///< Major semi-axis of ellipse

    /** Return analytical electric field value for i-th point near the hemisphere */
    Vec3 get_analyt_field(const int i, const Point3& origin) const;

    /** Return analytical potential value for i-th point near the hemisphere */
    double get_analyt_potential(const int i, const Point3& origin) const;

    /** Get analytical field enhancement for hemi-ellipsoid on infinite surface */
    double get_analyt_enhancement() const;
};

/** Class to interpolate current densities and temperatures */
class HeatReader: public SolutionReader {
public:
    /** HeatReader constructors */
    HeatReader(TriangleInterpolator* tri);
    HeatReader(TetrahedronInterpolator* tet);
    HeatReader(TriangleInterpolator* tri, TetrahedronInterpolator* tet);

    /** Interpolate solution on medium atoms using the solution on tetrahedral mesh nodes */
    void interpolate(const Medium &medium, const double empty_val, const int component, const bool srt);

    /** Linearly interpolate currents and temperatures in the bulk.
     *  In case of empty interpolator, constant values are stored. */
    void interpolate(fch::CurrentsAndHeating<3>& ch_solver, const double empty_val, const int component, const bool srt);

    /** Linearly interpolate currents and temperatures on the bulk surface.
     *  In case of empty interpolator, constant values are stored. */
    void interpolate_2d(fch::CurrentsAndHeating<3>& ch_solver, const double empty_val, const int component, const bool srt);

    /** Export interpolated temperature */
    void export_temperature(const int n_atoms, double* T);

    Vec3 get_rho(const int i) const;

    double get_rho_norm(const int i) const;

    double get_temperature(const int i) const;

private:
};

/** Class to calculate field emission effects with GETELEC */
class EmissionReader: public SolutionReader {
public:
    /** EmissionReader constructors. */
    EmissionReader(TriangleInterpolator* tri, const FieldReader& fields, const HeatReader& heat,
            const TetgenFaces& faces);
    EmissionReader(TetrahedronInterpolator* tet, const FieldReader& fields,
            const HeatReader& heat, const TetgenFaces& faces);
    EmissionReader(TriangleInterpolator* tri, TetrahedronInterpolator* tet,
            const FieldReader& fields, const HeatReader& heat, const TetgenFaces& faces);

    /** Calculates the emission currents and Nottingham heat distributions, including a rough
     * estimation of the space charge effects.
     * @param ch_solver heat solver object where J and Nottingham BCs will be written
     * @param workfunction Work function
     * @param Vappl Applied voltage (required for space charge calculations)
     */
    void transfer_emission(fch::CurrentsAndHeating<3>& ch_solver, const double workfunction,
            const double Vappl, bool blunt = false);

    double get_multiplier() const {return multiplier;}
    void set_multiplier(double _multiplier) { multiplier = _multiplier;}

private:
    /** Prepares the line inputted to GETELEC.
     *
     * @param point Starting point of the line
     * @param direction Direction of the line
     * @param rmax Maximum distance that the line extends
     */
    void emission_line(const Point3& point, const Vec3& direction, const double rmax);

    /** Calculates representative quantities Jrep and Frep for space charge calculations
     * (See https://arxiv.org/abs/1710.00050 for definitions)
     */
    void calc_representative();

    /**
     * Initialises class data.
     */
    void initialize();

    /**
     * Calculates electron emission distribution for a given configuration (
     * @param workfunction Input work function.
     */
    void calc_emission(double workfunction, bool blunt  = false);


    const double angstrom_per_nm = 10.0;
    const double nm2_per_angstrom2 = 0.01;

    const FieldReader& fields;    ///< Object containing the field on centroids of hex interface faces.
    const HeatReader& heat;       ///< Object containing the temperature on centroids of hexahedral faces.
    const TetgenFaces& faces;     ///< Object containing information on the faces of the mesh.

    vector<double> currents;    ///< Vector containing the emitted current density on the interface faces [in Amps/A^2].
    vector<double> nottingham; ///< Same as currents for nottingham heat deposition [in W/A^2]

    const int n_lines = 32; ///< Number of points in the line for GETELEC
    vector<double> rline;   ///< Line distance from the face centroid (passed into GETELEC)
    vector<double> Vline;   ///< Potential on the straight line (complements rline)

    double multiplier;      ///< Multiplier for the field for Space Charge.
    double Jmax;    ///< Maximum current density of the emitter [in amps/A^2]
    double Fmax = 0.;    ///< Maximum local field on the emitter [V/A]
    double Frep = 0.;    ///< Representative local field (used for space charge equation) [V/A]
    double Jrep = 0.;    ///< Representative current deinsity for space charge. [amps/A^2]

};

/** Class to calculate charges from electric field */
class ChargeReader: public SolutionReader {
public:
    /** ChargeReader constructors */
    ChargeReader(TriangleInterpolator* ip);
    ChargeReader(TetrahedronInterpolator* ip);
    ChargeReader(TriangleInterpolator* tri, TetrahedronInterpolator* tet);

    /** Calculate charge on the triangular faces using interpolated solution on the face centroid */
    void calc_interpolated_charges(const TetgenMesh& mesh, const double E0);

    /** Calculate charge on the triangular faces using direct solution on the face centroid */
    void calc_charges(const TetgenMesh& mesh, const double E0);

    /** Remove the atoms and their solutions outside the MD box */
    void clean(const Medium::Sizes& sizes, const double latconst);

    /** Set parameters to calculate analytical solution */
    void set_check_params(const double Q_tot, const double limit_min, const double limit_max);

    /** Check whether charge is conserved within specified limits.
     * The check is disabled if lower and upper limits are the same. */
    bool check_limits(const vector<Solution>* solutions=NULL) const;

    /** Get electric field on the centroid of i-th triangle */
    Vec3 get_elfield(const int i) const;

    /** Get area of i-th triangle */
    double get_area(const int i) const;

    /** Get charge of i-th triangle */
    double get_charge(const int i) const;

private:
    const double eps0 = 0.0055263494; ///< vacuum permittivity [e/V*A]
    double Q_tot;  ///< Analytical total charge
};

/** Class to calculate forces from charges and electric fields */
class ForceReader: public SolutionReader {
public:
    /** ChargeReader constructors */
    ForceReader(TriangleInterpolator* ip);
    ForceReader(TetrahedronInterpolator* ip);
    ForceReader(TriangleInterpolator* tri, TetrahedronInterpolator* tet);

    /** Calculate forces from atomic electric fields and face charges */
    void distribute_charges(const FieldReader &fields, const ChargeReader& faces,
        const double r_cut, const double smooth_factor);

    void calc_forces(const FieldReader &fields, TriangleInterpolator& ti);

    int calc_voronoi_charges(VoronoiMesh& mesh, const vector<int>& atom2surf, const FieldReader& fields,
             const double radius, const double latconst, const string& mesh_quality);

    /** Export the induced charge and force on imported atoms
     * @param n_atoms  number of first atoms field is calculated
     * @param xq       charge and force in PARCAS format (xq[0] = q1, xq[1] = Fx1, xq[2] = Fy1, xq[3] = Fz1, xq[4] = q2, xq[5] = Fx2 etc)
     */
    void export_force(const int n_atoms, double* xq);

    Vec3 get_force(const int i) const;

    double get_force_norm(const int i) const;

    double get_charge(const int i) const;

private:
    const double eps0 = 0.0055263494; ///< vacuum permittivity [e/V*A]
    const double force_factor = 0.5;  ///< force_factor = force / (charge * elfield)

    /** Remove cells with too big faces*/
    void clean_voro_faces(VoronoiMesh& mesh);

    int calc_voronois(VoronoiMesh& mesh, vector<bool>& atom_in_nanotip, const vector<int>& atom2face,
            const double radius, const double latconst, const string& mesh_quality);
};

} // namespace femocs

#endif /* SOLUTIONREADER_H_ */
