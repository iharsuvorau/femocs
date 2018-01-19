/*
 * Medium.cpp
 *
 *  Created on: 21.1.2016
 *      Author: veske
 */

#include "Medium.h"
#include <float.h>
#include <fstream>
#include <numeric>

#if USE_CGAL
#include <CGAL/hilbert_sort.h>
#endif

using namespace std;
namespace femocs {

Medium::Medium() {
    init_statistics();
    reserve(0);
}

Medium::Medium(const int n_atoms) {
    init_statistics();
    reserve(n_atoms);
}

// Sort the atoms by their cartesian or radial coordinate
void Medium::sort_atoms(const int coord, const string& direction) {
    require(coord >= 0 && coord <= 3, "Invalid coordinate: " + to_string(coord));

    if (size() < 2) return;

    if (coord == 3) {
        Point2 origin(sizes.xmid, sizes.ymid);
        for (int i = 0; i < size(); ++i)
            set_marker(i, 10000 * origin.distance2(get_point2(i)));
        if (direction == "up" || direction == "asc")
            sort(atoms.begin(), atoms.end(), Atom::sort_marker_up());
        else
            sort(atoms.begin(), atoms.end(), Atom::sort_marker_down());
    }

    else if (direction == "up" || direction == "asc")
        sort(atoms.begin(), atoms.end(), Atom::sort_up(coord));
    else if (direction == "down" || direction == "desc")
        sort(atoms.begin(), atoms.end(), Atom::sort_down(coord));
}

// Sort the atoms first by 1st and then by 2nd cartesian coordinate
void Medium::sort_atoms(const int x1, const int x2, const string& direction) {
    require(x1 >= 0 && x1 <= 2 && x2 >= 0 && x2 <= 2, "Invalid coordinates: " + to_string(x1) + ", " + to_string(x2));

    if (direction == "up" || direction == "asc")
        sort( atoms.begin(), atoms.end(), Atom::sort_up2(x1, x2) );
    else if (direction == "down" || direction == "desc")
        sort( atoms.begin(), atoms.end(), Atom::sort_down2(x1, x2) );
}

// Perform spatial sorting by ordering atoms along Hilbert curve
void Medium::sort_spatial() {
#if USE_CGAL
    CGAL::hilbert_sort( atoms.begin(), atoms.end(), Atom::sort_spatial(), CGAL::Hilbert_sort_middle_policy() );
#endif
}

// Reserve memory for data vectors
void Medium::reserve(const int n_atoms) {
    require(n_atoms >= 0, "Invalid number of atoms: " + to_string(n_atoms));
    atoms.clear();
    atoms.reserve(n_atoms);
}

// Reserve memory for data vectors
void Medium::resize(const int n_atoms) {
    require(n_atoms >= 0, "Invalid number of atoms: " + to_string(n_atoms));
    atoms.reserve(n_atoms);
}

// Define the addition of two Mediums
Medium& Medium::operator +=(const Medium &m) {
    atoms.insert(atoms.end(), m.atoms.begin(), m.atoms.end());
    calc_statistics();
    return *this;
}

// Add atom to atoms vector
void Medium::append(const Atom& atom) {
    expect((unsigned)size() < atoms.capacity(), "Allocated vector size exceeded!");
    atoms.push_back(atom);
}

// Add atom with defalt id and marker to atoms vector
void Medium::append(const Point3& point) {
    expect((unsigned)size() < atoms.capacity(), "Allocated vector sizes exceeded!");
    atoms.push_back(Atom(-1, point, 0));
}

// Initialize statistics about Medium
void Medium::init_statistics() {
    sizes.xmin = sizes.ymin = sizes.zmin = DBL_MAX;
    sizes.xmax = sizes.ymax = sizes.zmax =-DBL_MAX;
    sizes.xmean = sizes.ymean = sizes.zmean = 0.0;
    sizes.xmid = sizes.ymid = sizes.zmid = 0.0;

    sizes.xbox = sizes.ybox = sizes.zbox = 0;
    sizes.zminbox = DBL_MAX;
    sizes.zmaxbox =-DBL_MAX;
}

// Calculate the statistics about Medium
void Medium::calc_statistics() {
    int n_atoms = size();
    init_statistics();

    if (n_atoms <= 0) {
        expect(false, "Can't calculate statistics for empty set of atoms!");
        return;
    }

    Point3 average(0,0,0);

    // Find min and max coordinates
    for (int i = 0; i < n_atoms; ++i) {
        Point3 point = get_point(i);
        average += point;
        sizes.xmin = min(sizes.xmin, point.x);
        sizes.xmax = max(sizes.xmax, point.x);
        sizes.ymin = min(sizes.ymin, point.y);
        sizes.ymax = max(sizes.ymax, point.y);
        sizes.zmin = min(sizes.zmin, point.z);
        sizes.zmax = max(sizes.zmax, point.z);
    }

    // Define average coordinates
    average *= (1.0 / n_atoms);
    sizes.xmean = average.x;
    sizes.ymean = average.y;
    sizes.zmean = average.z;

    // Define size of simubox
    sizes.xbox = sizes.xmax - sizes.xmin;
    sizes.ybox = sizes.ymax - sizes.ymin;
    sizes.zbox = sizes.zmax - sizes.zmin;
    sizes.zminbox = sizes.zmin;
    sizes.zmaxbox = sizes.zmax;

    // Define the centre of simubox
    sizes.xmid = (sizes.xmax + sizes.xmin) / 2;
    sizes.ymid = (sizes.ymax + sizes.ymin) / 2;
    sizes.zmid = (sizes.zmax + sizes.zmin) / 2;
}

// Get number of atoms in Medium
int Medium::size() const {
    return atoms.size();
}

// Get 2-dimensional coordinates of i-th atom
Point2 Medium::get_point2(const int i) const {
    require(i >= 0 && i < size(), "Index out of bounds: " + to_string(i));
    return Point2(atoms[i].point.x, atoms[i].point.y);
}

// Get 3-dimensional coordinates of i-th atom
Point3 Medium::get_point(const int i) const {
    require(i >= 0 && i < size(), "Index out of bounds: " + to_string(i) + "/" + to_string(size()));
    return atoms[i].point;
}

// Get atom ID
int Medium::get_id(const int i) const {
    require(i >= 0 && i < size(), "Index out of bounds: " + to_string(i));
    return atoms[i].id;
}

// Get atom marker
int Medium::get_marker(const int i) const {
    require(i >= 0 && i < size(), "Index out of bounds: " + to_string(i));
    return atoms[i].marker;
}

// Get i-th Atom
Atom Medium::get_atom(const int i) const {
    require(i >= 0 && i < size(), "Index out of bounds: " + to_string(i));
    return atoms[i];
}

// Set entry to id-s vector
void Medium::set_id(const int i, const int id) {
    require(i >= 0 && i < size(), "Index out of bounds: " + to_string(i));
    atoms[i].id = id;
}

// Set entry to point-s vector
void Medium::set_point(const int i, const Point3& p) {
    require(i >= 0 && i < size(), "Index out of bounds: " + to_string(i));
    atoms[i].point = p;
}

// Set entry to x coordinate vector
void Medium::set_x(const int i, const double x) {
    require(i >= 0 && i < size(), "Index out of bounds: " + to_string(i));
    atoms[i].point.x = x;
}

// Set entry to y coordinate vector
void Medium::set_y(const int i, const double y) {
    require(i >= 0 && i < size(), "Index out of bounds: " + to_string(i));
    atoms[i].point.y = y;
}

// Set entry to z coordinate vector
void Medium::set_z(const int i, const double z) {
    require(i >= 0 && i < size(), "Index out of bounds: " + to_string(i));
    atoms[i].point.z = z;
}

// Set atom marker
void Medium::set_marker(const int i, const int m) {
    require(i >= 0 && i < size(), "Index out of bounds: " + to_string(i));
    atoms[i].marker = m;
}

// Pick the suitable write function based on the file type
// Function is active only when file write is enabled
void Medium::write(const string &file_name) const {
    if (!MODES.WRITEFILE) return;
    
    int n_atoms = size();
    expect(n_atoms > 0, "Zero atoms detected!");
    string ftype = get_file_type(file_name);
    
    ofstream outfile;
//    outfile << fixed;

    outfile.setf(std::ios::scientific);
    outfile.precision(6);

    if (ftype == "movie") outfile.open(file_name, ios_base::app);
    else outfile.open(file_name);
    require(outfile.is_open(), "Can't open a file " + file_name);
    
    if (ftype == "xyz" || ftype == "movie")
        write_xyz(outfile, n_atoms);
    else if (ftype == "vtk")
        write_vtk(outfile, n_atoms);
    else if (ftype == "ckx")
        write_ckx(outfile, n_atoms);
    else    
        require(false, "Unsupported file type: " + ftype);

    outfile.close();
}

// Compile data string from the data vectors
string Medium::get_data_string(const int i) const {
    if(i < 0) return "Medium properties=id:I:1:pos:R:3:marker:I:1";

    ostringstream strs; strs << fixed;
    strs << atoms[i];
    return strs.str();
}

// Output atom data in .xyz format
void Medium::write_xyz(ofstream& out, const int n_atoms) const {
    out << n_atoms << "\n";
    out << get_data_string(-1) << endl;

    for (int i = 0; i < n_atoms; ++i)
        out << get_data_string(i) << endl;
}

// Output atom data in .vtk format
void Medium::write_vtk(ofstream& out, const int n_atoms) const {
    out << "# vtk DataFile Version 3.0\n";
    out << "# Medium data\n";
    out << "ASCII\n";
    out << "DATASET UNSTRUCTURED_GRID\n\n";

    // Output the point coordinates
    out << "POINTS " << n_atoms << " double\n";
    for (int i = 0; i < n_atoms; ++i)
        out << get_point(i) << "\n";

    get_cell_data(out);
}

// Output atom data in .ckx format
void Medium::write_ckx(ofstream &out, const int n_atoms) const {
    out << n_atoms << "\n";
    out << "Medium properties=type:I:1:pos:R:3" << endl;
    for (int i = 0; i < n_atoms; ++i)
        out << atoms[i].marker << " " << atoms[i].point << endl;
}

// Get scalar and vector data associated with vtk cells
void Medium::get_cell_data(ofstream& out) const {
    const int celltype = 1;     // type of the cell in vtk format; 1-vertex, 10-tetrahedron
    const int dim = 1;          // number of vertices in the cell
    const int n_cells = size();
    const int n_atoms = size();

    // Output the vertices
    out << "\nCELLS " << n_cells << " " << (1+dim) * n_cells << "\n";
    for (int i = 0; i < n_cells; ++i)
        out << dim << " " << i << "\n";

    // Output cell types
    out << "\nCELL_TYPES " << n_cells << "\n";
    for (int i = 0; i < n_cells; ++i)
        out << celltype << "\n";

    out << "\nPOINT_DATA " << n_atoms << "\n";

    // write IDs of atoms
    out << "SCALARS ID int\nLOOKUP_TABLE default\n";
    for (int i = 0; i < n_atoms; ++i)
        out << atoms[i].id << "\n";

    // write atom markers
    out << "SCALARS marker int\nLOOKUP_TABLE default\n";
    for (int i = 0; i < n_atoms; ++i)
        out << atoms[i].marker << "\n";
}

// Copy statistics from another Medium
void Medium::copy_statistics(const Medium& m) {
    require(this->sizes.size() == m.sizes.size() , "Incompatible statistics!");
    for (int i = 0; i < m.sizes.size(); ++i)
        (&this->sizes.xmin)[i] = (&m.sizes.xmin)[i];
}

} /* namespace femocs */
