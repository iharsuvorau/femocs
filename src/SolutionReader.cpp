/*
 * SolutionReader.cpp
 *
 *  Created on: 6.6.2016
 *      Author: veske
 */

#include "SolutionReader.h"
#include "Macros.h"
#include "getelec.h"

#include <float.h>
#include <stdio.h>

using namespace std;
namespace femocs {

/* ==========================================
 * ============= SOLUTION READER ============
 * ========================================== */

// Initialize SolutionReader
SolutionReader::SolutionReader() : vec_label("vec"), vec_norm_label("vec_norm"), scalar_label("scalar"),
        empty_val(0), interpolator(NULL) {
    reserve(0);
}

SolutionReader::SolutionReader(TetrahedronInterpolator* ip, const string& vec_lab, const string& vec_norm_lab, const string& scal_lab) :
        vec_label(vec_lab), vec_norm_label(vec_norm_lab), scalar_label(scal_lab), empty_val(0), interpolator(ip) {
    reserve(0);
}

// Linearly interpolate solution on system atoms using tetrahedral interpolator
void SolutionReader::calc_interpolation(const double r_cut, const int component, const bool srt) {
    require(component >= 0 && component <= 2, "Invalid interpolation component: " + to_string(component));
    require(interpolator, "NULL interpolator cannot be used!");

    const int n_atoms = size();
    if (interpolator->size() == 0) {
        interpolation = vector<Solution>(n_atoms, Solution(empty_val));
        return;
    }

    // Sort atoms into sequential order to speed up interpolation
    if (srt) sort_spatial();

    // Enable or disable the search of points slightly outside the tetrahedra
    interpolator->search_outside(srt);

    int elem = 0;
    for (int i = 0; i < n_atoms; ++i) {
        Point3 point = get_point(i);
        // Find the element that contains (elem >= 0) or is closest (elem < 0) to the point
        elem = interpolator->locate_cell(point, abs(elem));

        // Store whether the point is in- or outside the mesh
        set_marker(i, elem < 0);

        // Calculate the interpolation
        if      (component == 0) interpolation.push_back( interpolator->interp_solution(point, abs(elem)) );
        else if (component == 1) interpolation.push_back( interpolator->interp_vector(point, abs(elem)) );
        else if (component == 2) interpolation.push_back( interpolator->interp_scalar(point, abs(elem)) );
    }

    // Apply histogram cleaner for the solution
    clean(0, r_cut);  // clean by vector x-component
    clean(1, r_cut);  // clean by vector y-component
    clean(2, r_cut);  // clean by vector z-component
    clean(3, r_cut);  // clean by vector norm
    clean(4, r_cut);  // clean by scalar

    // Sort atoms back to their initial order
    if (srt) {
        for (int i = 0; i < n_atoms; ++i)
            interpolation[i].id = atoms[i].id;

        sort( interpolation.begin(), interpolation.end(), Solution::sort_up() );
        sort( atoms.begin(), atoms.end(), Atom::sort_id() );
    }
}

// Reserve memory for solution vectors
void SolutionReader::reserve(const int n_nodes) {
    require(n_nodes >= 0, "Invalid number of nodes: " + to_string(n_nodes));

    atoms.clear();
    interpolation.clear();

    atoms.reserve(n_nodes);
    interpolation.reserve(n_nodes);
}

// Append solution
void SolutionReader::append_interpolation(const Solution& s) {
    expect(interpolation.size() < interpolation.capacity(), "Allocated vector size exceeded!");
    interpolation.push_back(s);
}

// Get i-th Solution
Solution SolutionReader::get_interpolation(const int i) const {
    require(i >= 0 && i < static_cast<int>(interpolation.size()), "Index out of bounds: " + to_string(i));
    return interpolation[i];
}

// Set i-th Solution
void SolutionReader::set_interpolation(const int i, const Solution& s) {
    require(i >= 0 && i < static_cast<int>(interpolation.size()), "Index out of bounds: " + to_string(i));
    interpolation[i] = s;
}

// Compile data string from the data vectors for file output
string SolutionReader::get_data_string(const int i) const{
    if (i < 0) return "SolutionReader properties=id:I:1:pos:R:3:marker:I:1:force:R:3:" + vec_norm_label + ":R:1:" + scalar_label + ":R:1";

    ostringstream strs; strs << fixed;
    strs << atoms[i] << " " << interpolation[i];
    return strs.str();
}

// Get information about data vectors for .vtk file
void SolutionReader::get_point_data(ofstream& out) const {
    const int n_atoms = size();

    // write IDs of atoms
    out << "SCALARS id int\nLOOKUP_TABLE default\n";
    for (int i = 0; i < n_atoms; ++i)
        out << atoms[i].id << "\n";

    // write atom markers
    out << "SCALARS marker int\nLOOKUP_TABLE default\n";
    for (int i = 0; i < n_atoms; ++i)
        out << atoms[i].marker << "\n";

    // output scalar (electric potential, temperature etc)
    out << "SCALARS " << scalar_label << " double\nLOOKUP_TABLE default\n";
    for (int i = 0; i < n_atoms; ++i)
        out << interpolation[i].scalar << "\n";

    // output vector magnitude explicitly to make it possible to apply filters in ParaView
    out << "SCALARS " << vec_norm_label << " double\nLOOKUP_TABLE default\n";
    for (int i = 0; i < n_atoms; ++i)
        out << interpolation[i].norm << "\n";

    // output vector data (electric field, current density etc)
    out << "VECTORS " << vec_label << " double\n";
    for (int i = 0; i < n_atoms; ++i)
        out << interpolation[i].vector << "\n";
}

// Get average electric field around I-th solution point
Solution SolutionReader::get_average_solution(const int I, const double r_cut) {
    // Cut off weights after 5 sigma
    const double r_cut2 = pow(5 * r_cut, 2);
    const double smooth_factor = r_cut / 5.0;

    Vec3 elfield(0.0);
    double potential = 0.0;

    Point3 point1 = get_point(I);
    double w_sum = 0.0;

    // E_I = sum i!=I [E_i * a*exp( -b*distance(i,I) )] / sum i!=I [a*exp( -b*distance(i,I) )]
    for (int i = 0; i < size(); ++i)
        if (i != I) {
            double dist2 = point1.distance2(get_point(i));
            if (dist2 > r_cut2 || interpolation[i].norm >= interpolator->error_field) continue;

            double w = exp(-1.0 * sqrt(dist2) / smooth_factor);
            w_sum += w;
            elfield += interpolation[i].vector * w;
            potential += interpolation[i].scalar * w;
        }

    if (w_sum > 0) {
        elfield *= (1.0 / w_sum); potential /= w_sum;
        return Solution(elfield, potential);
    }

    expect(false, "Node " + to_string(I) + " can't be averaged!");
    return(interpolation[I]);
}

// Get histogram for electric field x,y,z component or for its norm
void SolutionReader::get_histogram(vector<int> &bins, vector<double> &bounds, const int coordinate) {
    require(coordinate >= 0 && coordinate <= 4, "Invalid component: " + to_string(coordinate));

    const int n_atoms = size();
    const int n_bins = bins.size();
    const int n_bounds = bounds.size();

    // Find minimum and maximum values from all non-error values
    double value_min = DBL_MAX;
    double value_max =-DBL_MAX;
    double value;
    for (int i = 0; i < n_atoms; ++i) {
        if (coordinate == 4) value = interpolation[i].scalar;
        else if (coordinate == 3) value = interpolation[i].norm;
        else                 value = interpolation[i].vector[coordinate];

        if (fabs(value) < interpolator->error_field) {
            value_min = min(value_min, value);
            value_max = max(value_max, value);
        }
    }

    // Fill the bounds with values value_min:value_step:(value_max + epsilon)
    // Epsilon is added to value_max to include the maximum value in the up-most bin
    double value_step = (value_max - value_min) / n_bins;
    for (int i = 0; i < n_bounds; ++i)
        bounds[i] = value_min + value_step * i;
    bounds[n_bounds-1] += 1e-5 * value_step;

    for (int i = 0; i < n_atoms; ++i)
        for (int j = 0; j < n_bins; ++j) {
            if (coordinate == 4) value = interpolation[i].scalar;
            else if (coordinate == 3) value = interpolation[i].norm;
            else                 value = interpolation[i].vector[coordinate];

            if (value >= bounds[j] && value < bounds[j+1]) {
                bins[j]++;
                break;
            }
        }
}

// Clean the interpolation from peaks using histogram cleaner
void SolutionReader::clean(const int coordinate, const double r_cut) {
    require(coordinate >= 0 && coordinate <= 4, "Invalid coordinate: " + to_string(coordinate));
    require(interpolator, "NULL interpolator cannot be used!");
    const int n_atoms = size();
    const int n_bins = static_cast<int>(n_atoms / 250);

    if (n_bins <= 1 || r_cut < 0.1) return;

    vector<int> bins(n_bins, 0);
    vector<double> bounds(n_bins+1);
    get_histogram(bins, bounds, coordinate);

    // Find the first bin with zero entries from positive edge of bounds;
    // this will determine the maximum allowed elfield value
    double value_max = bounds[n_bins];
    for (int i = n_bins-1; i >= 0; --i) {
        if (bounds[i] < 0) break;
        if (bins[i] == 0) value_max = bounds[i];
    }

    // Find the last bin with zero entries from negative edge of bounds;
    // this will determine the minimum allowed elfield value
    double value_min = bounds[0];
    for (int i = 0; i < n_bins; ++i) {
        if (bounds[i+1] >= 0) break;
        if (bins[i] == 0) value_min = bounds[i+1];
    }

    require(value_min <= value_max, "Error in histogram cleaner!");

//    cout.precision(3);
//    cout << endl << coordinate << " " << value_min << " " << value_max << endl;
//    for (int i = 0; i < bins.size(); ++i) cout << bins[i] << " ";
//    cout << endl;
//    for (int i = 0; i < bounds.size(); ++i) cout << bounds[i] << " ";
//    cout << endl;

    // If all the bins are filled, no blocking will be applied
    if (value_min == bounds[0] && value_max == bounds[n_bins])
        return;

    double value;
    for (int i = 0; i < n_atoms; ++i) {
        if (coordinate < 3)  value = fabs(interpolation[i].vector[coordinate]);
        else if (coordinate == 3) value = fabs(interpolation[i].norm);
        else if (coordinate == 4) value = fabs(interpolation[i].scalar);

        if (value < value_min || value > value_max)
            interpolation[i] = get_average_solution(i, r_cut);
    }
}

// Compare interpolated scalar statistics with a constant
void SolutionReader::print_statistics(const double Q) {
    if (!MODES.VERBOSE) return;
    double q = 0;
    for (int i = 0; i < size(); ++i)
        q += interpolation[i].scalar;

    stringstream stream; stream << fixed << setprecision(3);
    stream << "Q / sum(" << scalar_label << ") = " << Q << " / " << q << " = " << Q/q;
    write_verbose_msg(stream.str());
}

// Print statistics about interpolated solution
void SolutionReader::print_statistics() {
    if (!MODES.VERBOSE) return;

    const int n_atoms = size();
    Vec3 vec(0), rms_vec(0);
    double scalar = 0, rms_scalar = 0;

    for (int i = 0; i < n_atoms; ++i) {
        Vec3 v = interpolation[i].vector;
        double s = interpolation[i].scalar;

        vec += v; rms_vec += v * v;
        scalar += s; rms_scalar += s * s;
    }

    vec *= (1.0 / n_atoms);
    rms_vec = Vec3(sqrt(rms_vec.x), sqrt(rms_vec.y), sqrt(rms_vec.z)) * (1.0 / n_atoms);
    scalar = scalar / n_atoms;
    rms_scalar = sqrt(rms_scalar) / n_atoms;

    stringstream stream;
    stream << "mean " << vec_label << ": \t" << vec;
    stream << "\n   rms " << vec_label << ": \t" << rms_vec;
    stream << "\n  mean & rms " << scalar_label << ": " << scalar << "\t" << rms_scalar;
    write_verbose_msg(stream.str());
}

// Initialise statistics about the solution
void SolutionReader::init_statistics() {
    stat.vec_norm_min = stat.scal_min = DBL_MAX;
    stat.vec_norm_max = stat.scal_max = -DBL_MAX;
}

// Calculate statistics about the solution
void SolutionReader::calc_statistics() {
    init_statistics();

    for (int i = 0; i < size(); ++i) {
        double norm = interpolation[i].norm;
        double scalar = interpolation[i].scalar;
        stat.vec_norm_max = max(stat.vec_norm_max, norm);
        stat.vec_norm_min = min(stat.vec_norm_min, norm);
        stat.scal_max = max(stat.scal_max, scalar);
        stat.scal_min = min(stat.scal_min, scalar);
    }
}

/* ==========================================
 * ============== FIELD READER ==============
 * ========================================== */

FieldReader::FieldReader() : SolutionReader(), radius1(0), radius2(0), E0(0), interpolator3(NULL) {}

FieldReader::FieldReader(TriangleInterpolator* ip) : SolutionReader(NULL, "elfield", "elfield_norm", "potential"),
        radius1(0), radius2(0), E0(0), interpolator3(ip) {}

FieldReader::FieldReader(TetrahedronInterpolator* ip) : SolutionReader(ip, "elfield", "elfield_norm", "potential"),
        radius1(0), radius2(0), E0(0), interpolator3(NULL) {}

// Linearly interpolate solution on system atoms using triangular interpolator
void FieldReader::calc_interpolation2D(const double r_cut, const int component, const bool srt) {
    require(component >= 0 && component <= 2, "Invalid interpolation component: " + to_string(component));
    require(interpolator3, "NULL interpolator cannot be used!");

    const int n_atoms = size();
    if (interpolator3->size() == 0) {
        interpolation = vector<Solution>(n_atoms, Solution(empty_val));
        return;
    }

    // Sort atoms into sequential order to speed up interpolation
    if (srt) sort_spatial();

    // Disable the search of points outside the triangles
    interpolator3->search_outside(false);

    int elem = 0;
    for (int i = 0; i < n_atoms; ++i) {
        Point3 point = get_point(i);
        // Find the element that contains (elem >= 0) or is closest (elem < 0) to the point
        elem = interpolator3->locate_cell(point, abs(elem));

        // Store whether the point is in- or outside the mesh
        set_marker(i, elem < 0);

        // Calculate the interpolation
        if      (component == 0) interpolation.push_back( interpolator3->interp_solution(point, abs(elem)) );
        else if (component == 1) interpolation.push_back( interpolator3->interp_vector(point, abs(elem)) );
        else if (component == 2) interpolation.push_back( interpolator3->interp_scalar(point, abs(elem)) );
    }

    // Apply histogram cleaner for the solution
    // TODO cleaners require the presence of interpolator
//    clean(0, r_cut);  // clean by vector x-component
//    clean(1, r_cut);  // clean by vector y-component
//    clean(2, r_cut);  // clean by vector z-component
//    clean(3, r_cut);  // clean by vector norm
//    clean(4, r_cut);  // clean by scalar

    // Sort atoms back to their initial order
    if (srt) {
        for (int i = 0; i < n_atoms; ++i)
            interpolation[i].id = atoms[i].id;

        sort( interpolation.begin(), interpolation.end(), Solution::sort_up() );
        sort( atoms.begin(), atoms.end(), Atom::sort_id() );
    }
}

// Linearly interpolate solution on Medium atoms using triangular interpolator
void FieldReader::interpolate2D(const Medium &medium, const double r_cut, const int component, const bool srt) {
    const int n_atoms = medium.size();

    // store the atom coordinates
    reserve(n_atoms);
    for (int i = 0; i < n_atoms; ++i)
        append( Atom(i, medium.get_point(i), 0) );

    // interpolate solution
    calc_interpolation2D(r_cut, component, srt);

    // store the original atom id-s
    for (int i = 0; i < n_atoms; ++i)
        atoms[i].id = medium.get_id(i);
}

// Linearly interpolate electric field on a set of points using triangular interpolator
void FieldReader::interpolate2D(const int n_points, const double* x, const double* y, const double* z,
        const double r_cut, const int component, const bool srt) {

    // store the point coordinates
    reserve(n_points);
    for (int i = 0; i < n_points; ++i)
        append(Atom(i, Point3(x[i], y[i], z[i]), 0));

    // interpolate solution
    calc_interpolation2D(r_cut, component, srt);
}

// Linearly interpolate solution on Medium atoms
void FieldReader::interpolate(const Medium &medium, const double r_cut, const int component, const bool srt) {
    const int n_atoms = medium.size();
    
    // store the atom coordinates
    reserve(n_atoms);
    for (int i = 0; i < n_atoms; ++i)
        append( Atom(i, medium.get_point(i), 0) );

    // interpolate solution
    calc_interpolation(r_cut, component, srt);

    // store the original atom id-s
    for (int i = 0; i < n_atoms; ++i)
        atoms[i].id = medium.get_id(i);
}

// Linearly interpolate electric field on a set of points
void FieldReader::interpolate(const int n_points, const double* x, const double* y, const double* z,
        const double r_cut, const int component, const bool srt) {

    // store the point coordinates
    reserve(n_points);
    for (int i = 0; i < n_points; ++i)
        append(Atom(i, Point3(x[i], y[i], z[i]), 0));

    // interpolate solution
    calc_interpolation(r_cut, component, srt);
}

// Linearly interpolate electric field for the currents and temperature solver
void FieldReader::transfer_elfield(fch::CurrentsAndHeatingStationary<3>* ch_solver, const double r_cut, const bool srt) {
    // import the surface nodes the solver needs
    vector<dealii::Point<3>> nodes;
    ch_solver->get_surface_nodes(nodes);

    const int n_nodes = nodes.size();

    // store the node coordinates
    reserve(n_nodes);
    int i = 0;
    for (dealii::Point<3>& node : nodes)
        append( Atom(i++, Point3(node[0], node[1], node[2]), 0) );

    // interpolate solution on the nodes
    calc_interpolation(r_cut, 1, srt);

    // export electric field norms to the solver
    vector<double> elfields(n_nodes);
    for (int i = 0; i < n_nodes; ++i)
        elfields[i] = 10.0 * get_elfield_norm(i);
    ch_solver->set_electric_field_bc(elfields);
}

// Linearly interpolate electric field for the currents and temperature solver
void FieldReader::transfer_elfield(fch::CurrentsAndHeating<3>& ch_solver, const double r_cut, const bool srt) {
    // import the surface nodes the solver needs
    vector<dealii::Point<3>> nodes;
    ch_solver.get_surface_nodes(nodes);

    const int n_nodes = nodes.size();

    // store the node coordinates
    reserve(n_nodes);
    int i = 0;
    for (dealii::Point<3>& node : nodes)
        append( Atom(i++, Point3(node[0], node[1], node[2]), 0) );

    // interpolate solution on the nodes
    calc_interpolation(r_cut, 1, srt);

    // export electric field norms to the solver
    vector<double> elfields(n_nodes);
    for (int i = 0; i < n_nodes; ++i)
        elfields[i] = 10.0 * get_elfield_norm(i);
    ch_solver.set_electric_field_bc(elfields);
}

// Linearly interpolate electric field on a set of points
void FieldReader::export_elfield(const int n_points, double* Ex, double* Ey, double* Ez, double* Enorm, int* flag) {
    require(n_points == size(), "Invalid query size: " + to_string(n_points));
    for (int i = 0; i < n_points; ++i) {
        Ex[i] = interpolation[i].vector.x;
        Ey[i] = interpolation[i].vector.y;
        Ez[i] = interpolation[i].vector.z;
        Enorm[i] = interpolation[i].norm;
        flag[i] = atoms[i].marker;
    }
}

// Linearly interpolate electric potential on a set of points
void FieldReader::export_potential(const int n_points, double* phi, int* flag) {
    require(n_points == size(), "Invalid query size: " + to_string(n_points));
    for (int i = 0; i < n_points; ++i) {
        phi[i] = interpolation[i].scalar;
        flag[i] = atoms[i].marker;
    }
}

// Export interpolated electric field
void FieldReader::export_solution(const int n_atoms, double* Ex, double* Ey, double* Ez, double* Enorm) {
    if (n_atoms <= 0) return;

    // Initially pass the zero electric field for all the atoms
    for (int i = 0; i < n_atoms; ++i) {
        Ex[i] = 0;
        Ey[i] = 0;
        Ez[i] = 0;
        Enorm[i] = 0;
    }

    // Pass the the calculated electric field for stored atoms
    for (int i = 0; i < size(); ++i) {
        int id = get_id(i);
        if (id < 0 || id >= n_atoms) continue;

        Ex[id] = interpolation[i].vector.x;
        Ey[id] = interpolation[i].vector.y;
        Ez[id] = interpolation[i].vector.z;
        Enorm[id] = interpolation[i].norm;
    }
}

Vec3 FieldReader::get_elfield(const int i) const {
    require(i >= 0 && i < size(), "Invalid index: " + to_string(i));
    return interpolation[i].vector;
}

double FieldReader::get_elfield_norm(const int i) const {
    require(i >= 0 && i < size(), "Invalid index: " + to_string(i));
    return interpolation[i].norm;
}

double FieldReader::get_potential(const int i) const {
    require(i >= 0 && i < size(), "Invalid index: " + to_string(i));
    return interpolation[i].scalar;
}

// Set parameters for calculating analytical solution
void FieldReader::set_analyt(const double E0, const double radius1, const double radius2) {
    this->E0 = E0;
    this->radius1 = radius1;
    if (radius2 > radius1)
        this->radius2 = radius2;
    else
        this->radius2 = radius1;
}

double FieldReader::get_enhancement() const {
    double Emax = -1e100;
    for (Solution s : interpolation)
        Emax = max(Emax, s.norm);

    return fabs(Emax / E0);
}

double FieldReader::get_analyt_enhancement() const {
    expect(radius1 > 0, "Invalid minor semi-axis: " + to_string(radius1));

    if ( radius2 <= radius1 )
        return 3.0;
    else {
        double nu = radius2 / radius1;
        double zeta = sqrt(nu*nu - 1);
        return pow(zeta, 3.0) / (nu * log(zeta + nu) - zeta);
    }
}

void FieldReader::print_enhancement() const {
    double gamma1 = get_enhancement();
    double gamma2 = get_analyt_enhancement();

    stringstream stream;
    stream << fixed << setprecision(3);
    stream << "field enhancements:  Femocs:" << gamma1
            << "  analyt:" << gamma2
            << "  f-a:" << gamma1-gamma2
            << "  f/a:" << gamma1/gamma2;

    write_verbose_msg(stream.str());
}

/* ==========================================
 * =============== HEAT READER ==============
 * ========================================== */

HeatReader::HeatReader() : SolutionReader() {}
HeatReader::HeatReader(TetrahedronInterpolator* ip) : SolutionReader(ip, "rho", "rho_norm", "temperature") {}

// Linearly interpolate solution on Medium atoms
void HeatReader::interpolate(const Medium &medium, const double r_cut, const int component, const bool srt) {
    const int n_atoms = medium.size();

    // store the atom coordinates
    reserve(n_atoms);
    for (int i = 0; i < n_atoms; ++i)
        append( Atom(i, medium.get_point(i), 0) );

    // interpolate solution
    calc_interpolation(r_cut, component, srt);

    // restore the original atom id-s
    for (int i = 0; i < n_atoms; ++i)
        atoms[i].id = medium.get_id(i);
}

// Linearly interpolate electric field for the currents and temperature solver
// In case of empty interpolator, constant values are stored
void HeatReader::interpolate(fch::CurrentsAndHeating<3>& ch_solver, const double empty_val,
        const double r_cut, const int component, const bool srt) {
    this->empty_val = empty_val;

    // import the surface nodes the solver needs
    vector<dealii::Point<3>> nodes;
    ch_solver.get_surface_nodes(nodes);

    // store the node coordinates
    reserve(nodes.size());
    int i = 0;
    for (dealii::Point<3>& node : nodes)
        append( Atom(i++, Point3(node[0], node[1], node[2]), 0) );

    // interpolate solution on the nodes
    calc_interpolation(r_cut, component, srt);
}

// Export interpolated temperature
void HeatReader::export_temperature(const int n_atoms, double* T) {
    if (n_atoms <= 0) return;

    // Pass the the calculated temperature for stored atoms
    for (int i = 0; i < size(); ++i) {
        int identifier = get_id(i);
        if (identifier < 0 || identifier >= n_atoms) continue;
        T[identifier] = get_temperature(i);
    }
}

Vec3 HeatReader::get_rho(const int i) const {
    require(i >= 0 && i < size(), "Invalid index: " + to_string(i));
    return interpolation[i].vector;
}

double HeatReader::get_rho_norm(const int i) const {
    require(i >= 0 && i < size(), "Invalid index: " + to_string(i));
    return interpolation[i].norm;
}

double HeatReader::get_temperature(const int i) const {
    require(i >= 0 && i < size(), "Invalid index: " + to_string(i));
    return interpolation[i].scalar;
}

/* ==========================================
 * ============= EMISSION READER ============
 * ========================================== */

EmissionReader::EmissionReader() : SolutionReader() {}
EmissionReader::EmissionReader(TetrahedronInterpolator* ip) : SolutionReader(ip, "none", "rho_norm", "temperature") {}

double EmissionReader::get_rho_norm(const int i) const {
    require(i >= 0 && i < size(), "Invalid index: " + to_string(i));
    return interpolation[i].norm;
}

double EmissionReader::get_temperature(const int i) const {
    require(i >= 0 && i < size(), "Invalid index: " + to_string(i));
    return interpolation[i].scalar;
}

void EmissionReader::emission_line(const Point3& point, const Vec3& direction, const double rmax,
                        vector<double> &rline, vector<double> &Vline) {

    const double nm_per_angstrom = 0.1;
    const double rmin = 0.0;
    const int n_lines = rline.size();
    Point3 pfield(direction.x, direction.y, direction.z);

    FieldReader fr(interpolator);
    fr.reserve(n_lines);

    for (int i = 0; i < n_lines; i++){
        rline[i] = rmin + ((rmax - rmin) * i) / (n_lines - 1);
        fr.append(point - pfield * rline[i]);
    }
    fr.calc_interpolation(0, 0, false);
    for (int i = 0; i < n_lines; i++){
        Vline[i] = fr.get_potential(i);
        rline[i] *= nm_per_angstrom;
    }

    for (int i = 0; i < n_lines; i++){
        Vline[i] -= Vline[0];
        rline[i] -= rline[0];
    }

    for (int i = 1; i < n_lines; ++i) { // go through points
        if (Vline[i] < Vline[i-1]) { // if decreasing at a point
            double dVdx = 0.0;
            int j;
            for (j = i + 1; j < n_lines; ++j) {
                if (Vline[j] > Vline[i-1]) {
                    dVdx = (Vline[j] - Vline[i-1]) / (rline[j] - rline[i-1]);
                    break;
                }
            }

            if (dVdx == 0.0) {
                if (i > 1)
                    dVdx = (Vline[i-1] - Vline[i-2]) / (rline[i-1] - rline[i-2]);
                else
                    write_verbose_msg("Non-monotonous Vline could not be recovered at i = " + to_string(i));
            }
            for (int k = 0; k <= j; ++k)
                Vline[k] =  Vline[i-1] + (rline[k] - rline[i-1]) * dVdx;
        }
    }
}

void EmissionReader::transfer_emission(fch::CurrentsAndHeating<3>& ch_solver, const FieldReader& fields,
        const double workfunction, const HeatReader& heat_reader) {

    const double angstrom_per_nm = 10.0;
    const double nm2_per_angstrom2 = 0.01;

    const int n_nodes = fields.size();
    const int n_lines = 32;
    vector<double> currents(n_nodes), nottingham(n_nodes);
    vector<double> rline(n_lines), Vline(n_lines);

    reserve(n_nodes);
    atoms = fields.atoms;

    double Fmax = 0.0;
    double Jmax = 0.0;

    struct emission gt;
    gt.W = workfunction;    // set workfuntion, must be set in conf. script
    gt.R = 200.0;   // radius of curvature (overrided by femocs potential distribution)
    gt.gamma = 10;  // enhancement factor (overrided by femocs potential distribution)

    for (int i = 0; i < n_nodes; ++i)
        Fmax = max(Fmax, fields.get_elfield_norm(i));
    Fmax *= angstrom_per_nm;

    for (int i = 0; i < n_nodes; ++i) {
        Vec3 field = fields.get_elfield(i);
        gt.mode = 0;
        gt.F = angstrom_per_nm * field.norm();
        gt.Temp = heat_reader.get_temperature(i);

        if (gt.F > 0.6 * Fmax){
            field.normalize();
            emission_line(get_point(i), field, 16.0 * gt.W / gt.F, rline, Vline);
            gt.Nr = n_lines;
            gt.xr = &rline[0];
            gt.Vr = &Vline[0];
            gt.mode = -21;
        }
        gt.approx = 0;
        cur_dens_c(&gt);
        if (gt.ierr != 0 )
            write_verbose_msg("GETELEC 1st call returned with error, ierr = " + to_string(gt.ierr));

        if (gt.Jem > 0.1 * Jmax){
            gt.approx = 1;
            cur_dens_c(&gt);
            if (gt.ierr != 0 )
                write_verbose_msg("GETELEC 2nd call returned with error, ierr = " + to_string(gt.ierr));
        }

        Jmax = max(Jmax, gt.Jem);
        currents[i] = nm2_per_angstrom2 * gt.Jem;
        nottingham[i] = nm2_per_angstrom2 * gt.heat;
        append_interpolation( Solution(Vec3(0), log(currents[i]), log(fabs(nottingham[i]))) );
    }

    ch_solver.set_emission_bc(currents, nottingham);
}

/* ==========================================
 * ============== CHARGE READER =============
 * ========================================== */

ChargeReader::ChargeReader() : SolutionReader() {}
ChargeReader::ChargeReader(TetrahedronInterpolator* ip) : SolutionReader(ip, "elfield", "area", "charge") {}

// Calculate charges on surface faces using interpolated electric fields
// Conserves charge worse but gives smoother forces
void ChargeReader::calc_interpolated_charges(const TetgenMesh& mesh, const double E0) {
    const double sign = fabs(E0) / E0;
    const int n_faces = mesh.faces.size();

    // Store the centroids of the triangles
    reserve(n_faces);
    for (int i = 0; i < n_faces; ++i)
        append( Atom(i, mesh.faces.get_centroid(i), 0) );

    // Interpolate electric field on the centroids of triangles
    calc_interpolation(0, 1, true);

    // Calculate the charge from the found electric field and store it
    for (int i = 0; i < n_faces; ++i) {
        double area = mesh.faces.get_area(i);
        double charge = eps0 * area * interpolation[i].norm * sign;
        interpolation[i].norm = area;
        interpolation[i].scalar = charge;
    }
}

void ChargeReader::calc_charges(const TetgenMesh& mesh, const double E0) {
    const double sign = fabs(E0) / E0;
    const int n_faces = mesh.faces.size();
    const int n_quads_per_triangle = 3;

    // Store the centroids of the triangles
    reserve(n_faces);
    for (int i = 0; i < n_faces; ++i)
        append( Atom(i, mesh.faces.get_centroid(i), 0) );

    // create triangle index to its centroid index mapping
    vector<int> tri2centroid(n_faces);
    for (int face = 0; face < n_faces; ++face) {
        for (int node : mesh.quads[n_quads_per_triangle * face])
            if (mesh.nodes.get_marker(node) == TYPES.FACECENTROID) {
                tri2centroid[face] = node;
                break;
            }
    }

    // Calculate the charges for the triangles
    for (int face = 0; face < n_faces; ++face) {
        double area = mesh.faces.get_area(face);
        Vec3 elfield = interpolator->get_vector(tri2centroid[face]);
        double charge = eps0 * area * elfield.norm() * sign;
        append_interpolation(Solution(elfield, area, charge));
//        append_interpolation(Solution(mesh.faces.get_norm(face), area, charge));
    }
}

// Remove the atoms and their solutions outside the box
void ChargeReader::clean(const Medium::Sizes& sizes, const double latconst) {
    const int n_atoms = size();
    const int eps = latconst / 2.0;;
    vector<bool> in_box; in_box.reserve(n_atoms);

    // Check the locations of points
    for (int i = 0; i < n_atoms; ++i) {
        Point3 point = get_point(i);
        const bool box_x = point.x >= (sizes.xmin - eps) && point.x <= (sizes.xmax + eps);
        const bool box_y = point.y >= (sizes.ymin - eps) && point.y <= (sizes.ymax + eps);
        const bool box_z = point.z >= (sizes.zmin - eps) && point.z <= (sizes.zmax + eps);
        in_box.push_back(box_x && box_y && box_z);
    }

    const int n_box = vector_sum(in_box);
    vector<Atom> _atoms; _atoms.reserve(n_box);
    vector<Solution> _interpolation; _interpolation.reserve(n_box);

    // Copy the solutions and atoms that remain into box
    for (int i = 0; i < n_atoms; ++i)
        if (in_box[i]) {
            _atoms.push_back(atoms[i]);
            _interpolation.push_back(interpolation[i]);
        }
    atoms = _atoms;
    interpolation = _interpolation;
}

// Check whether charge is conserved within specified limits
bool ChargeReader::charge_conserved(const double Q, const double eps) const {
    double q = 0;
    for (int i = 0; i < size(); ++i)
        q += interpolation[i].scalar;
    q = Q / q;

    return q >= (1 - eps) && q <= (1 + eps);
}

Vec3 ChargeReader::get_elfield(const int i) const {
    require(i >= 0 && i < size(), "Invalid index: " + to_string(i));
    return interpolation[i].vector;
}

double ChargeReader::get_area(const int i) const {
    require(i >= 0 && i < size(), "Invalid index: " + to_string(i));
    return interpolation[i].norm;
}

double ChargeReader::get_charge(const int i) const {
    require(i >= 0 && i < size(), "Invalid index: " + to_string(i));
    return interpolation[i].scalar;
}

/* ==========================================
 * ============== FORCE READER ==============
 * ========================================== */

ForceReader::ForceReader() : SolutionReader() {}
ForceReader::ForceReader(TetrahedronInterpolator* ip) : SolutionReader(ip, "force", "force_norm", "charge") {}

// Extract the surface atoms whose Voronoi cells are exposed to vacuum
int ForceReader::calc_voronois(VoronoiMesh& voromesh, vector<bool>& node_in_nanotip,
        const double radius, const double latconst, const string& mesh_quality)
{
    const int n_this_nodes = size();
    const double radius2 = radius * radius;
    Medium::calc_statistics();

    // Make map for atoms in nanotip
    Point2 centre(sizes.xmid, sizes.ymid);
    node_in_nanotip = vector<bool>(n_this_nodes);
    for (int i = 0; i < n_this_nodes; ++i)
        node_in_nanotip[i] = centre.distance2(get_point2(i)) <= radius2;

    const int n_nanotip_nodes = vector_sum(node_in_nanotip);

    // Separate nanotip from substrate
    Medium nanotip(n_nanotip_nodes);
    for (int i = 0; i < n_this_nodes; ++i)
        if (node_in_nanotip[i])
            nanotip.append(get_atom(i));
    nanotip.calc_statistics();

    // Generate Voronoi cells around the nanotip
    // r - reconstruct, v - output Voronoi cells, Q - quiet, q - mesh quality
    if (voromesh.generate(nanotip, latconst, "rQq" + mesh_quality, "vQ"))
        return 1;

    // Clean the mesh from faces and cells that have node in the infinity
    voromesh.clean();

    voromesh.nodes.write("out/voro_nodes.vtk");
    voromesh.vfaces.write("out/voro_faces.vtk");
    voromesh.voros.write("out/voro_cells.vtk");

    return n_nanotip_nodes;
}

// Extract the surface atoms whose Voronoi cells are exposed to vacuum
bool ForceReader::calc_voronoi_charges(const double radius, const double latconst, const string& mesh_quality) {
    const int n_this_nodes = size();

    // Extract nanotip and generate Voronoi cells around it
    VoronoiMesh voromesh;
    vector<bool> node_in_nanotip;
    const int n_nanotip_nodes = calc_voronois(voromesh, node_in_nanotip, radius, latconst, mesh_quality);
    const int n_voronoi_nodes = voromesh.nodes.size();

    require(voromesh.nodes.size() > 0, "Empty Voronoi mesh cannot be handled!");
    require(voromesh.voros.size() > 0, "Empty Voronoi mesh cannot be handled!");

    // Extract the nodes that are near the surface
    vector<bool> node_near_surface(n_voronoi_nodes);
    for (int i = 0; i < n_nanotip_nodes; ++i)
        for (VoronoiFace face : voromesh.voros[i])
            node_near_surface[face.nborcell(i)] = true;

    // the potential exactly on surface is zero, therefore no need to interpolate there
    for (int i = 0; i < n_nanotip_nodes; ++i)
        node_near_surface[i] = false;

    Medium interpolation_points(vector_sum(node_near_surface));
    for (int i = 0; i < n_voronoi_nodes; ++i)
        if (node_near_surface[i])
            interpolation_points.append(voromesh.nodes[i]);

    // interpolate electric potential on those points
    FieldReader fr(interpolator);
    fr.interpolate(interpolation_points, 0, 2, true);

    // find potentials in the middle of all Voronoi cells
    vector<double> potentials(n_voronoi_nodes);
    int index = 0;
    for (int i = n_nanotip_nodes; i < n_voronoi_nodes; ++i)
        if (node_near_surface[i])
            potentials[i] = fr.get_potential(index++);

    // calculate Voronoi cell charges
    int cell_index = -1;
    for (int i = 0; i < n_this_nodes; ++i)
        if (node_in_nanotip[i]) {
            double charge = 0;
            Point3 centre = get_point(i);
            for (VoronoiFace face : voromesh.voros[++cell_index]) {
                int nbor_cell = face.nborcell(cell_index);
                double phi = potentials[nbor_cell];
                if (phi > 0)
                    charge -= phi * face.area().norm() / centre.distance(voromesh.nodes[nbor_cell]);
            }
            interpolation[i].scalar = charge;
        }

    return 0;
}

// Replace the charge and force on the nanotip nodes with the one found with Voronoi cells
void ForceReader::recalc_forces(const FieldReader &fields, const vector<Vec3>& areas) {
    require(static_cast<int>(areas.size()) == fields.size(), "Mismatch of data sizes: "
        + to_string(areas.size()) + " vs " + to_string(fields.size()) );

    for (size_t i = 0; i < areas.size(); ++i) {
        if (areas[i] == 0) continue;
        
        Vec3 field = fields.get_elfield(i);
        double charge = areas[i].dotProduct(field) * eps0; // [e]
        Vec3 force = field * (charge * force_factor);      // [e*V/A]
        interpolation[i] = Solution(force, charge);
    }
}

// Calculate forces from atomic electric fields and face charges
void ForceReader::calc_forces(const FieldReader &fields, TriangleInterpolator& ti) {
    const int n_atoms = fields.size();

    // Copy the atom data
    reserve(n_atoms);
    atoms = fields.atoms;

    // Calculate the charges by ensuring the that the total sum of it remains conserved
    vector<double> charges;
    ti.interp_conserved(charges, atoms);

    // calculate forces and store them
    for (int i = 0; i < n_atoms; ++i) {
        Vec3 force = fields.get_elfield(i) * (charges[i] * force_factor);   // [e*V/A]
        interpolation.push_back(Solution(force, charges[i]));
    }
}

void ForceReader::calc_forces_vol2(const TetgenMesh& mesh, const FieldReader &fields, const ChargeReader &face_charges,
        TriangleInterpolator& tri_interpolator, const double r_cut, const double smooth_factor) {
    const int n_atoms = fields.size();
    const int n_faces = tri_interpolator.get_n_cells();

    tri_interpolator.search_outside(false);

    // Copy the atom data
    reserve(n_atoms);
    atoms = fields.atoms;

    vector<double> charges(n_atoms);
    vector<double> weights;

    // calculate the neighbour list for triangles
    vector<vector<int>> neighbours(n_faces);
    for (int i = 0; i < n_faces; ++i) {
        SimpleFace sface = mesh.faces[i];
        for (int j = i+1; j < n_faces; ++j)
            if ( sface.edge_neighbor(mesh.faces[j]) ) {
                neighbours[i].push_back(j);
                neighbours[j].push_back(i);
            }
    }

    // make the connection between the faces and the points that are close to them
    vector<vector<int>> face2atoms(n_faces);
    int face = 0;
    for (int atom = 0; atom < n_atoms; ++atom) {
        face = abs( tri_interpolator.locate_cell(get_point(atom), face) );
        set_marker(atom, face);

        // face that is below the point
        require(face >= 0 && face < n_faces, "Invalid face: " + to_string(face));
        face2atoms[face].push_back(atom);
        // faces that are nearest neighbours of the face below the point
        for (int f : neighbours[face])
            face2atoms[f].push_back(atom);
    }

    // loop through all the faces
    for (int face = 0; face < face_charges.size(); ++face) {
        if (face2atoms[face].size() == 0) continue;

        Point3 centroid = face_charges.get_point(face);
        double q_face = face_charges.get_charge(face);
        double sf = smooth_factor * sqrt(mesh.faces.get_area(face));

        // Find weights and normalization factor for all the atoms for given face
        weights = vector<double>(n_atoms);
        double w_sum = 0.0;
        // loop through all the atoms that are close to the face
        for (int atom : face2atoms[face]) {
            double dist2 = centroid.periodic_distance2(get_point(atom), sizes.xbox, sizes.ybox);
            double w = exp(-1.0 * sqrt(dist2) / sf);
            weights[atom] = w;
            w_sum += w;
        }

        // If there were any atoms connected to the face, store the partial charges on atoms
        if (w_sum > 0) {
            w_sum = 1.0 / w_sum;
            for (int atom = 0; atom < n_atoms; ++atom)
                if (weights[atom] > 0)
                    charges[atom] += weights[atom] * w_sum * q_face;
        }
    }

    for (int atom = 0; atom < n_atoms; ++atom) {
        Vec3 force = fields.get_elfield(atom) * (charges[atom] * force_factor);   // [e*V/A]
        interpolation.push_back(Solution(force, charges[atom]));
    }

    clean(0, r_cut);  // clean by vector x-component
    clean(1, r_cut);  // clean by vector y-component
    clean(2, r_cut);  // clean by vector z-component
    clean(3, r_cut);  // clean by vector norm
    clean(4, r_cut);  // clean by scalar
}

// Calculate forces from atomic electric fields and face charges
void ForceReader::calc_forces(const FieldReader &fields, const ChargeReader& faces,
        const double r_cut, const double smooth_factor) {

    const int n_atoms = fields.size();
    const int n_faces = faces.size();

    // Copy the atom data
    reserve(n_atoms);
    for (int i = 0; i < n_atoms; ++i)
        append(fields.get_atom(i));

    calc_statistics();

    /* Distribute the charges on surface faces between surface atoms.
     * If q_i and Q_j is the charge on i-th atom and j-th face, respectively, then
     *     q_i = sum_j(w_ij * Q_j),  sum_i(w_ij) = 1 for every j
     * where w_ij is the weight of charge on j-th face for the i-th atom. */

    vector<double> charges(n_atoms);
    vector<double> weights;
    for (int face = 0; face < n_faces; ++face) {
        Point3 point1 = faces.get_point(face);
        double q_face = faces.get_charge(face);

        double r_cut2 = faces.get_area(face) * 100.0;
        double sf = smooth_factor * sqrt(r_cut2) / 10.0;

        // Find weights and normalization factor for all the atoms for given face
        // Get the charge for real surface atoms
        weights = vector<double>(n_atoms);
        double w_sum = 0.0;
        for (int atom = 0; atom < n_atoms; ++atom) {
            double dist2 = point1.periodic_distance2(get_point(atom), sizes.xbox, sizes.ybox);
            if (dist2 > r_cut2) continue;

            double w = exp(-1.0 * sqrt(dist2) / sf);
            weights[atom] = w;
            w_sum += w;
        }

        // Store the partial charges on atoms
        w_sum = 1.0 / w_sum;
        for (int atom = 0; atom < n_atoms; ++atom)
            if (weights[atom] > 0)
                charges[atom] += weights[atom] * w_sum * q_face;

    }

    for (int atom = 0; atom < n_atoms; ++atom) {
        Vec3 force = fields.get_elfield(atom) * (charges[atom] * force_factor);   // [e*V/A]
        interpolation.push_back(Solution(force, charges[atom]));
    }

    clean(0, r_cut);  // clean by vector x-component
    clean(1, r_cut);  // clean by vector y-component
    clean(2, r_cut);  // clean by vector z-component
    clean(3, r_cut);  // clean by vector norm
    clean(4, r_cut);  // clean by scalar
}

// Export the induced charge and force on imported atoms
void ForceReader::export_force(const int n_atoms, double* xq) {
    if (n_atoms <= 0) return;

    // Initially pass the zero force and charge for all the atoms
    for (int i = 0; i < 4*n_atoms; ++i)
        xq[i] = 0;

    // Pass the the calculated electric field for stored atoms
    for (int i = 0; i < size(); ++i) {
        int identifier = get_id(i);
        if (identifier < 0 || identifier >= n_atoms) continue;

        identifier *= 4;
        xq[identifier++] = interpolation[i].scalar;
        for (double x : interpolation[i].vector)
            xq[identifier++] = x;
    }
}

Vec3 ForceReader::get_force(const int i) const {
    require(i >= 0 && i < size(), "Invalid index: " + to_string(i));
    return interpolation[i].vector;
}

double ForceReader::get_force_norm(const int i) const {
    require(i >= 0 && i < size(), "Invalid index: " + to_string(i));
    return interpolation[i].norm;
}

double ForceReader::get_charge(const int i) const {
    require(i >= 0 && i < size(), "Invalid index: " + to_string(i));
    return interpolation[i].scalar;
}

} // namespace femocs
