/*
 * Media.cpp
 *
 *  Created on: 5.4.2016
 *      Author: veske
 */

#include "Media.h"

using namespace std;
namespace femocs {

// =================================================
//             Implementation of Vacuum
// =================================================

// Constructor of Vacuum class
Vacuum::Vacuum() :
        Medium() {
}

// Generates Vacuum by adding four points to the top of simulation cell
const void Vacuum::generate_simple(const AtomReader::Sizes* sizes) {
    int M = 4; // total number of nodes
    reserve(M);

    // Add points to the xy-plane corners on current layer
    add_atom( Atom(-1, Point3(sizes->xmax, sizes->ymax, sizes->zmaxbox), 0) );
    add_atom( Atom(-1, Point3(sizes->xmax, sizes->ymin, sizes->zmaxbox), 0) );
    add_atom( Atom(-1, Point3(sizes->xmin, sizes->ymax, sizes->zmaxbox), 0) );
    add_atom( Atom(-1, Point3(sizes->xmin, sizes->ymin, sizes->zmaxbox), 0) );

    init_statistics();
    calc_statistics();
}

// =================================================
//              Implementation of Bulk
// =================================================

// Constructor for Bulk class
Bulk::Bulk(const double latconst, const int nnn) :
        Medium() {
    crys_struct.latconst = latconst;
    crys_struct.nnn = nnn;
}
Bulk::Bulk() : Bulk(0, 0) {};

// Function to make bulk material with nodes on surface and on by-hand made bottom coordinates
const void Bulk::extract_reduced_bulk(Surface* surf, const AtomReader::Sizes* sizes) {
    int i;
//    shared_ptr<Edge> edge(new Edge(crys_struct.latconst, crys_struct.nnn));
//    edge->extract_edge(surf, sizes);

    int n_surf = surf->get_n_atoms();
    int n_edge = 4; //edge->getN();

    // Reserve memory for bulk atoms and their parameters
    reserve(n_surf + n_edge);

    for (i = 0; i < n_surf; ++i)
        add_atom(surf->get_atom(i));

//    for (i = 0; i < n_edge; ++i)
//        add_atom(edge->get_atom(i));

    // Add extra atoms to the bottom corner edges of simulation cell
    add_atom( Atom(-1, Point3(sizes->xmin, sizes->ymin, sizes->zminbox), 0) );
    add_atom( Atom(-1, Point3(sizes->xmin, sizes->ymax, sizes->zminbox), 0) );
    add_atom( Atom(-1, Point3(sizes->xmax, sizes->ymin, sizes->zminbox), 0) );
    add_atom( Atom(-1, Point3(sizes->xmax, sizes->ymax, sizes->zminbox), 0) );

    calc_statistics();
}

// Function to make bulk material with nodes on surface and on by-hand made bottom coordinates
const void Bulk::generate_simple(const AtomReader::Sizes* sizes) {
    int M = 4; // total number of nodes
    reserve(M);

    // Add atoms to the bottom corner edges of simulation cell
    add_atom( Atom(-1, Point3(sizes->xmin, sizes->ymin, sizes->zminbox), TYPES.BULK) );
    add_atom( Atom(-1, Point3(sizes->xmin, sizes->ymax, sizes->zminbox), TYPES.BULK) );
    add_atom( Atom(-1, Point3(sizes->xmax, sizes->ymin, sizes->zminbox), TYPES.BULK) );
    add_atom( Atom(-1, Point3(sizes->xmax, sizes->ymax, sizes->zminbox), TYPES.BULK) );

    init_statistics();
    calc_statistics();
}

// Function to extract bulk material from input atomistic data
const void Bulk::extract_truncated_bulk_old(AtomReader* reader) {
    int i;
    const int N = reader->get_n_atoms();
    vector<bool> is_bulk(N);
    vector<bool> is_surf(N);

    // Get number and locations of bulk atoms
    for (i = 0; i < N; ++i)
        is_bulk[i] = (reader->get_point(i).z > reader->sizes.zminbox)
                && (reader->get_type(i) == TYPES.BULK
                        || reader->get_type(i) == TYPES.SURFACE);

    for (i = 0; i < N; ++i)
        is_surf[i] = (reader->get_coordination(i) > 0)
                && (reader->get_coordination(i) < crys_struct.nnn);

    reserve(vector_sum(is_bulk));

    // Add bulk atoms
    for (i = 0; i < N; ++i)
        if (is_bulk[i] && !is_surf[i])
            add_atom(reader->get_atom(i));

    // Add surface atoms
    for (i = 0; i < N; ++i)
        if (is_bulk[i] && is_surf[i])
            add_atom(reader->get_atom(i));

    calc_statistics();
}

// Function to extract bulk material from input atomistic data
const void Bulk::extract_truncated_bulk(AtomReader* reader) {
    int i;
    const int N = reader->get_n_atoms();
    vector<bool> is_bulk(N);

    // Get number and locations of bulk atoms
    for (i = 0; i < N; ++i)
        is_bulk[i] = (reader->get_point(i).z > reader->sizes.zminbox)
                && (reader->get_type(i) == TYPES.BULK);

    reserve(vector_sum(is_bulk));

    // Add bulk atoms
    for (i = 0; i < N; ++i)
        if (is_bulk[i])
            add_atom(reader->get_atom(i));

    calc_statistics();
}

// Function to extract bulk material from input atomistic data
const void Bulk::extract_bulk(AtomReader* reader) {
    int i;
    int N = reader->get_n_atoms();
    vector<bool> is_bulk(N);

    for (i = 0; i < N; ++i)
        is_bulk[i] = reader->get_type(i) != TYPES.VACANCY;

    reserve(vector_sum(is_bulk));

    for (i = 0; i < N; ++i)
        if (is_bulk[i])
            add_atom(reader->get_atom(i));

    calc_statistics();
}

// Function to extract bulk material from input atomistic data
const void Bulk::rectangularize(const AtomReader::Sizes* sizes, const double r_cut) {
    const double zmin_up = this->sizes.zmin + crys_struct.latconst / 2.1;
    const double zmin_down = this->sizes.zmin;

    for (int i = 0; i < get_n_atoms(); ++i) {
        Point3 point = get_point(i);

        // Flatten the atoms on bottom layer
        if ( point.z <= zmin_up ) set_z(i, zmin_down);

        // Flatten the atoms on the sides of simulation box
        if ( on_boundary(point.x, sizes->xmin, r_cut) ) set_x(i, sizes->xmin);
        else if ( on_boundary(point.y, sizes->ymin, r_cut) ) set_y(i, sizes->ymin);
        else if ( on_boundary(point.x, sizes->xmax, r_cut) ) set_x(i, sizes->xmax);
        else if ( on_boundary(point.y, sizes->ymax, r_cut) ) set_y(i, sizes->ymax);
    }
}

// =================================================
//            Implementation of Surface
// =================================================

// Constructor for Surface class
Surface::Surface(const double latconst, const int nnn) :
        Medium() {
    crys_struct.latconst = latconst;
    crys_struct.nnn = nnn;
    roughness = 0;
}
;
Surface::Surface() : Surface(0, 0) {};

// Generate Surface with 4 atoms at the corners of simulation cell
const void Surface::generate_simple(const AtomReader::Sizes* sizes, const double z) {
    // Reserve memory for atoms
    reserve(4);

    // Add 4 atoms to the corners of simulation cell
    add_atom( Atom(-1, Point3(sizes->xmin, sizes->ymin, z), 0) );
    add_atom( Atom(-1, Point3(sizes->xmax, sizes->ymin, z), 0) );
    add_atom( Atom(-1, Point3(sizes->xmax, sizes->ymax, z), 0) );
    add_atom( Atom(-1, Point3(sizes->xmin, sizes->ymax, z), 0) );
}

const void Surface::calc_statistics() {
    Medium::calc_statistics();
    roughness = (sizes.zmax - sizes.zmin) / 2.0;
}

const double Surface::get_roughness() {
    return roughness;
}

// Function to pick suitable extraction function
const void Surface::extract_surface(AtomReader* reader) {
    //if (reader->types.simu_type == "md")
    //    extract_by_coordination(reader);
    //else if (reader->types.simu_type == "kmc")
    extract_by_type(reader);

    calc_statistics();
}

// Extract surface by coordination analysis
const void Surface::extract_by_type(AtomReader* reader) {
    const int n_atoms = reader->get_n_atoms();
    vector<bool> is_surface(n_atoms);

    // Get number and locations of surface atoms
    for (int i = 0; i < n_atoms; ++i)
        is_surface[i] = (reader->get_type(i) == TYPES.SURFACE);

    // Preallocate memory for Surface atoms
    reserve(vector_sum(is_surface));

    // Add surface atoms to Surface
    for (int i = 0; i < n_atoms; ++i)
        if (is_surface[i])
            add_atom(reader->get_atom(i));
}

// Extract surface by coordination analysis
const void Surface::extract_by_coordination(AtomReader* reader) {
    int n_atoms = reader->get_n_atoms();
    int i;
    double zmin = reader->sizes.zminbox + crys_struct.latconst;

    vector<bool> is_surface(n_atoms);

    for (i = 0; i < n_atoms; ++i)
        is_surface[i] = (reader->get_point(i).z > zmin) && (reader->get_coordination(i) > 0)
                && (reader->get_coordination(i) < crys_struct.nnn);

    // Preallocate memory for Surface atoms
    reserve(vector_sum(is_surface));

    for (i = 0; i < n_atoms; ++i)
        if (is_surface[i])
            add_atom(reader->get_atom(i));
}

// Function to coarsen the atoms outside the cylinder in the middle of simulation cell
const Surface Surface::coarsen(const double coord_cutoff, const double r_in, const double r_out,
        const double coarse_factor, const AtomReader::Sizes* ar_sizes) {

    const double r_cut2 = r_in * r_in;

    int i, j;
    const int n_atoms = get_n_atoms();
    vector<bool> is_dense(n_atoms);

    Point2 origin2d((sizes.xmax + sizes.xmin) / 2, (sizes.ymax + sizes.ymin) / 2);
    Surface dense_surf(crys_struct.latconst, crys_struct.nnn);
    Surface coarse_surf(crys_struct.latconst, crys_struct.nnn);

    Edge edge;
    edge.extract_edge(this, ar_sizes, 2.0*crys_struct.latconst);

    // Add edge atoms to the beginning of coarse surface
    // to give them higher priority of survival during clean-up
    coarse_surf += edge;

    // Create a map from atoms in- and outside the dense region
    for (i = 0; i < n_atoms; ++i)
        is_dense[i] = (origin2d.distance2(get_point2(i)) < r_cut2);

    int n_dense_atoms = vector_sum(is_dense);
    int n_coarse_atoms = n_atoms - n_dense_atoms + edge.get_n_atoms();

    dense_surf.reserve(n_dense_atoms);
    coarse_surf.reserve(n_coarse_atoms);

    // Separate the atoms from dense and coarse regions
    for (i = 0; i < n_atoms; ++i) {
        if (is_dense[i])
            dense_surf.add_atom(get_atom(i));
        else
            coarse_surf.add_atom(get_atom(i));
    }

    // Among the other things calculate the average z-coordinate of coarse_surf atoms
    coarse_surf.calc_statistics();
    Point3 origin3d(origin2d[0], origin2d[1], coarse_surf.sizes.zmean);

    coarse_surf = coarse_surf.clean(origin3d, r_in, r_out, coarse_factor);
    dense_surf = dense_surf.clean_lonely_atoms(coord_cutoff);
    coarse_surf += dense_surf;

    return coarse_surf;
}

// Function to coarsen the atoms outside the cylinder in the middle of simulation cell
const Surface Surface::coarsen_vol2(const double coord_cutoff, const double r_in, const double r_out,
        const double coarse_factor, const AtomReader::Sizes* ar_sizes) {

    const double r_cut2 = r_in * r_in;

    int i, j;
    const int n_atoms = get_n_atoms();
    vector<bool> is_dense(n_atoms);

    Point2 origin2d((sizes.xmax + sizes.xmin) / 2, (sizes.ymax + sizes.ymin) / 2);
    Surface dense_surf(crys_struct.latconst, crys_struct.nnn);
    Surface coarse_surf(crys_struct.latconst, crys_struct.nnn);

    // Create a map from atoms in- and outside the dense region
    for (i = 0; i < n_atoms; ++i)
        is_dense[i] = (origin2d.distance2(get_point2(i)) < r_cut2);

    const int n_dense_atoms = vector_sum(is_dense);
    const int n_coarse_atoms = n_atoms - n_dense_atoms;

    dense_surf.reserve(n_dense_atoms);
    coarse_surf.reserve(n_coarse_atoms);

    // Separate the atoms from dense and coarse regions
    for (i = 0; i < n_atoms; ++i)
        if (is_dense[i])
            dense_surf.add_atom(get_atom(i));
        else
            coarse_surf.add_atom(get_atom(i));

    // Among the other things calculate the average z-coordinate of coarse_surf atoms
    coarse_surf.calc_statistics();
    Point3 origin3d(origin2d[0], origin2d[1], coarse_surf.sizes.zmean);

    Edge edge;
    edge.extract_edge(&coarse_surf, ar_sizes, 2.0*crys_struct.latconst);

    // Sort edge and coarse_surf atoms in radial direction
    // to get proper order in deleting atoms too close to each other

    edge.sort_atoms(Point2(origin3d.x, origin3d.y), "down");
    coarse_surf.sort_atoms(Point2(origin3d.x, origin3d.y), "down");
    dense_surf = dense_surf.clean_lonely_atoms(coord_cutoff);

    // Build up union surface atoms in the order of their survival priority during clean-up

    Surface union_surf(crys_struct.latconst, crys_struct.nnn);
    union_surf.generate_simple(ar_sizes, coarse_surf.sizes.zmean);
    union_surf += edge;
    union_surf += coarse_surf;

    union_surf = union_surf.clean(origin3d, r_in, r_out, coarse_factor);

    union_surf += dense_surf;

    return union_surf;
}

// Function to flatten the atoms on the sides of simulation box
const Surface Surface::rectangularize(const AtomReader::Sizes* sizes, const double r_cut) {
    Edge edge;
    edge.extract_edge(this, sizes, r_cut);

    Surface surf(crys_struct.latconst, crys_struct.nnn);
    surf += edge;
    surf.add(this);

    return surf.clean(crys_struct.latconst / 3.0);
}

// Function to clean the Surface from overlapping atoms
const Surface Surface::clean() {
    return this->clean(Point3(), 0, 1e20, 0);
}

// Function to clean the Surface from atoms with distance smaller than constant cutoff
const Surface Surface::clean(const double r_cut) {
    require(r_cut >= 0, "Cutoff distance between atoms must be non-negative!");
    return this->clean(Point3(), -1, 1e20, r_cut);
}

// Function to clean the Surface from atoms with distance smaller than radially increasing cutoff
const Surface Surface::clean(const Point3 &origin, double r_in, double r_out, double multiplier) {
    Surface surf(crys_struct.latconst, crys_struct.nnn);
    int i, j;
    double cutoff2 = 0.0;
    const double A2 = multiplier * crys_struct.latconst * multiplier * crys_struct.latconst;

    const bool use_non_constant_cutoff = multiplier > 0 && r_in >= 0;
    if (r_in < 0) cutoff2 = multiplier * multiplier;

    const int n_atoms = get_n_atoms();
    vector<bool> do_delete(n_atoms, false);

    // Loop through all the atoms
    for(i = 0; i < n_atoms-1; ++i) {
        // Skip already deleted atoms
        if (do_delete[i]) continue;

        Point3 point1 = get_point(i);

        // r_cutoff(r) = A * sqrt(r - r_in), if r >  r_in
        //             = 0,                  if r <= r_in
        if(use_non_constant_cutoff) {
            double dist = min(r_out, point1.distance(origin));
            cutoff2 = A2 * (dist - r_in);
        }

        for (j = i+1; j < n_atoms; ++j) {
            // Skip already deleted atoms
            if (do_delete[j]) continue;
            do_delete[j] = point1.distance2(get_point(j)) <= cutoff2;
        }
    }

    surf.reserve( n_atoms - vector_sum(do_delete) );
    for (i = 0; i < n_atoms; ++i)
        if(!do_delete[i])
            surf.add_atom(get_atom(i));

    surf.calc_statistics();
    return surf;
}

// Function to delete atoms that are separate from others
// Atom is considered lonely if its coordination is lower than coord_min
const Surface Surface::clean_lonely_atoms(const double r_cut) {
    const double cutoff2 = r_cut * r_cut;
    const int n_atoms = get_n_atoms();
    const int coord_min = 2;

    Surface surf(crys_struct.latconst, crys_struct.nnn);
    int i, j;

    vector<bool> atom_not_lonely(n_atoms, false);
    vector<int> coords(n_atoms, 0);

    // Loop through all the atoms
    for(i = 0; i < n_atoms-1; ++i) {
        // Skip already processed atoms
        if (atom_not_lonely[i]) continue;

        Point3 point1 = get_point(i);

        // Loop through the possible neighbours of the atom
        for (j = i+1; j < n_atoms; ++j) {
            // Skip already processed atoms
            if (atom_not_lonely[j]) continue;

            if (point1.distance2(get_point(j)) <= cutoff2) {
                atom_not_lonely[i] = ++coords[i] >= coord_min;
                atom_not_lonely[j] = ++coords[j] >= coord_min;
            }
        }
    }

    surf.reserve( vector_sum(atom_not_lonely) );
    for (i = 0; i < n_atoms; ++i)
        if (atom_not_lonely[i])
            surf.add_atom(get_atom(i));

    surf.calc_statistics();
    return surf;
}

// =================================================
//              Implementation of Edge
// =================================================

// Constructors for Edge class
Edge::Edge(const double latconst, const int nnn) :
        Medium() {
    crys_struct.latconst = latconst;
    crys_struct.nnn = nnn;
}
;
Edge::Edge() : Edge(0, 0) {};

// Exctract the atoms near the simulation box sides
const void Edge::extract_edge(Medium* atoms, const AtomReader::Sizes* sizes, const double r_cut) {
    const int n_atoms = atoms->get_n_atoms();

    // Reserve memory for atoms
    reserve(n_atoms);

    // Get the atoms from edge areas
    for (int i = 0; i < n_atoms; ++i) {
        Point3 point = atoms->get_point(i);
        const bool near1 = on_boundary(point.x, sizes->xmin, sizes->xmax, r_cut);
        const bool near2 = on_boundary(point.y, sizes->ymin, sizes->ymax, r_cut);

        if (near1 || near2)
            add_atom(atoms->get_atom(i));
    }

    // Loop through all the added atoms and flatten the atoms on the sides of simulation box
    for (int i = 0; i < get_n_atoms(); ++i) {
        Point3 point = get_point(i);
        if ( on_boundary(point.x, sizes->xmin, r_cut) ) set_x(i, sizes->xmin);
        if ( on_boundary(point.x, sizes->xmax, r_cut) ) set_x(i, sizes->xmax);
        if ( on_boundary(point.y, sizes->ymin, r_cut) ) set_y(i, sizes->ymin);
        if ( on_boundary(point.y, sizes->ymax, r_cut) ) set_y(i, sizes->ymax);
    }
}

const Edge Edge::clean(const double r_cut) {
    require(r_cut >= 0, "Cutoff distance between atoms must be non-negative!");
    return clean(-1, r_cut);
}

const Edge Edge::clean(const double r_cut, const double coarse_factor) {
    int i, j;
    int n_atoms = get_n_atoms();

    const double A2 = coarse_factor * crys_struct.latconst * coarse_factor * crys_struct.latconst;
    double cutoff2 = A2 * (1 + (sizes.xmax - sizes.xmin) / 2 - r_cut);
    if (r_cut < 0)
        cutoff2 = coarse_factor * coarse_factor;

    Point3 point1;

    Edge edge(crys_struct.latconst, crys_struct.nnn);
    vector<bool> do_delete(n_atoms, false);

    // Mark atoms that are too close and need to be deleted
    for (i = 0; i < (n_atoms - 1); ++i) {
        // Skip already deleted atoms
        if (do_delete[i]) continue;

        // Mark too close nodes
        point1 = get_point(i);
        for (j = i + 1; j < n_atoms; ++j) {
            // Skip already deleted atoms
            if (do_delete[j]) continue;

            if (point1.distance2(get_point(j)) <= cutoff2)
                do_delete[j] = true;
        }
    }

    // Reserve memory for edge atoms
    edge.reserve(n_atoms - vector_sum(do_delete));
    // Compile coarsened Edge
    for (i = 0; i < n_atoms; ++i)
        if (!do_delete[i])
            edge.add_atom(get_atom(i));

    edge.calc_statistics();
    return edge;
}

} /* namespace femocs */
