/*
 * Interpolator.h
 *
 *  Created on: 19.10.2016
 *      Author: veske
 */

#ifndef LINEARINTERPOLATOR_H_
#define LINEARINTERPOLATOR_H_

#include "Primitives.h"
#include "Medium.h"
#include "TetgenMesh.h"
#include "TetgenCells.h"
#include "Coarseners.h"
#include "laplace.h"
#include "currents_and_heating.h"
#include "currents_and_heating_stationary.h"

using namespace std;
namespace femocs {

template<int dim>
class LinearInterpolator : public Medium {
public:
    /** LinearInterpolator conctructor */
    LinearInterpolator(const TetgenMesh* m) : mesh(m), nodes(&m->nodes) {
        reserve(0);
        reserve_precompute(0);
    }
    
    virtual ~LinearInterpolator() {};

    // Add solution to solutions vector
    void append_solution(const Solution& solution) {
//        expect((unsigned)size() < solutions.capacity(), "Allocated vector size exceeded!");
        solutions.push_back(solution);
    }

    // Enable or disable the search of points slightly outside the tetrahedron
    void search_outside(const bool enable) {
        if (enable) {
            zero = -1.0 * epsilon;
            one = 1.0 + epsilon;
        } else {
            zero = 0;
            one = 1.0;
        }
    }

    // Return full solution on i-th node
    Solution get_solution(const int i) const {
        require(i >= 0 && i < size(), "Invalid index: " + to_string(i));
        return solutions[i];
    }

    // Return vector component of solution on i-th node
    Vec3 get_vector(const int i) const {
        require(i >= 0 && i < size(), "Invalid index: " + to_string(i));
        return solutions[i].vector;
    }

    // Return scalar component of solution on i-th node
    double get_scalar(const int i) const {
        require(i >= 0 && i < size(), "Invalid index: " + to_string(i));
        return solutions[i].scalar;
    }

    /** Electric field that is assigned to atoms not found from mesh.
     *  Its value is BIG to make it immediately visible from the dataset. */
    const double error_field = 1e20;

protected:
    /** Constants specifying the interpolation tolerances.
     * Making zero a bit negative allows searching points outside the tetrahedra. */
    const double epsilon = 0.1;
    double zero = -1.0 * epsilon;
    double one = 1.0 + epsilon;

    vector<Solution> solutions;     ///< interpolation data
    vector<vector<int>> neighbours; ///< nearest neighbours of the cells
    vector<Point3> centroids;       ///< cell centroid coordinates
    
    const TetgenMesh* mesh;         ///< Full mesh data Mesh with nodes and surface faces
    const TetgenNodes* nodes;
    virtual const TetgenCells<dim>* cells() const { return NULL; }

    /** Pre-compute data about cells to make interpolation faster */
    virtual void precompute() {}

    virtual int get_cell_type() const { return 0; }

    /** Reserve memory for pre-computation data */
    virtual void reserve_precompute(const int N) {
        neighbours = vector<vector<int>>(N);
        centroids.reserve(N);
    }

    /** Reserve memory for interpolation data */
    void reserve(const int N) {
        require(N >= 0, "Invalid number of points: " + to_string(N));
        atoms.clear();
        solutions.clear();

        atoms.reserve(N);
        solutions.reserve(N);
    }
    
    /** Get i-th entry from all data vectors; i < 0 gives the header of data vectors */
    string get_data_string(const int i) const {
        if (i < 0)
            return "LinearInterpolator properties=id:I:1:pos:R:3:marker:I:1:force:R:3:elfield_norm:R:1:potential:R:1";

        ostringstream strs;
        strs << atoms[i] << " " << solutions[i];
        return strs.str();
    }

    /** Get scalar and vector data associated with atoms */
    void get_cell_data(ofstream& out) const {
        const int celltype = get_cell_type();
        const int n_cells = cells()->size();
        const int n_atoms = size();

        // Output the vertex indices 
        out << "\nCELLS " << n_cells << " " << (1+dim) * n_cells << "\n";
        for (int i = 0; i < n_cells; ++i)
            out << dim << " " << (*cells())[i] << "\n";

        // Output cell types
        out << "\nCELL_TYPES " << n_cells << "\n";
        for (int i = 0; i < n_cells; ++i)
            out << celltype << "\n";

        out << "\nPOINT_DATA " << n_atoms << "\n";

        // write atom IDs
        out << "SCALARS ID int\nLOOKUP_TABLE default\n";
        for (int i = 0; i < n_atoms; ++i)
            out << atoms[i].id << "\n";

        // write atom markers
        out << "SCALARS marker int\nLOOKUP_TABLE default\n";
        for (int i = 0; i < n_atoms; ++i)
            out << atoms[i].marker << "\n";

        out << "SCALARS elfield_norm double\nLOOKUP_TABLE default\n";
        for (int i = 0; i < n_atoms; ++i)
            out << solutions[i].norm << "\n";

        out << "SCALARS potential double\nLOOKUP_TABLE default\n";
        for (int i = 0; i < n_atoms; ++i)
            out << solutions[i].scalar << "\n";
    }

    // Determinant of 3x3 matrix which's last column consists of ones
    double determinant(const Vec3 &v1, const Vec3 &v2) {
        return v1.x * (v2.y - v2.z) - v1.y * (v2.x - v2.z) + v1.z * (v2.x - v2.y);
    }

    // Determinant of 3x3 matrix which's columns consist of Vec3-s
    double determinant(const Vec3 &v1, const Vec3 &v2, const Vec3 &v3) {
        return v1.x * (v2.y * v3.z - v3.y * v2.z) - v2.x * (v1.y * v3.z - v3.y * v1.z)
                + v3.x * (v1.y * v2.z - v2.y * v1.z);
    }

    // Determinant of 4x4 matrix which's last column consists of ones
    double determinant(const Vec3 &v1, const Vec3 &v2, const Vec3 &v3, const Vec3 &v4) {
        const double det1 = determinant(v2, v3, v4);
        const double det2 = determinant(v1, v3, v4);
        const double det3 = determinant(v1, v2, v4);
        const double det4 = determinant(v1, v2, v3);

        return det4 - det3 + det2 - det1;
    }

    // Determinant of 4x4 matrix which's columns consist of Vec4-s
    double determinant(const Vec4 &v1, const Vec4 &v2, const Vec4 &v3, const Vec4 &v4) {
        double det1 = determinant(Vec3(v1.y, v1.z, v1.w), Vec3(v2.y, v2.z, v2.w),
                Vec3(v3.y, v3.z, v3.w));
        double det2 = determinant(Vec3(v1.x, v1.z, v1.w), Vec3(v2.x, v2.z, v2.w),
                Vec3(v3.x, v3.z, v3.w));
        double det3 = determinant(Vec3(v1.x, v1.y, v1.w), Vec3(v2.x, v2.y, v2.w),
                Vec3(v3.x, v3.y, v3.w));
        double det4 = determinant(Vec3(v1.x, v1.y, v1.z), Vec3(v2.x, v2.y, v2.z),
                Vec3(v3.x, v3.y, v3.z));

    return v4.w * det4 - v4.z * det3 + v4.y * det2 - v4.x * det1;
}
};

/** Class to linearly interpolate solution inside tetrahedral mesh */
/* Useful links
 *
 * Compact theory how to find barycentric coordintes (bbc):
 * http://steve.hollasch.net/cgindex/geometry/ptintet.html
 *
 * Properties of determinant:
 * http://www.vitutor.com/alg/determinants/properties_determinants.html
 * http://www.vitutor.com/alg/determinants/minor_cofactor.html
 *
 * c++ code to find and handle bcc-s:
 * http://dennis2society.de/painless-tetrahedral-barycentric-mapping
 *
 * Interpolating inside the element using bcc:
 * http://www.cwscholz.net/projects/diss/html/node37.html
 *
 */
class TetrahedronInterpolator : public LinearInterpolator<4> {
public:
    /** TetrahedronInterpolator conctructor */
    TetrahedronInterpolator(const TetgenMesh* mesh);

    /** Extract the electric potential and electric field values on the tetrahedra nodes from FEM solution */
    bool extract_solution(fch::Laplace<3>* laplace);

    /** Extract the current density and temperature values on the tetrahedra nodes from FEM solution */
    bool extract_solution(fch::CurrentsAndHeatingStationary<3>* fem);

    /** Extract the current density and temperature values on the tetrahedra nodes from FEM solution */
    bool extract_solution(fch::CurrentsAndHeating<3>* fem);

    /** Interpolate both vector and scalar data.
     * Function assumes, that tetrahedron, that surrounds the point, is previously already found with locate_element.
     * @param point  point where the interpolation is performed
     * @param elem   tetrahedron around which the interpolation is performed */
    Solution interp_solution(const Point3 &point, const int elem) const;

    /** Interpolate scalar data.
     * Function assumes, that tetrahedron, that surrounds the point, is previously already found with locate_element.
     * @param point  point where the interpolation is performed
     * @param elem   tetrahedron around which the interpolation is performed */
    Vec3 interp_vector(const Point3 &point, const int elem) const;

    /** Interpolate vector data.
     * Function assumes, that tetrahedron, that surrounds the point, is previously already found with locate_element.
     * @param point  point where the interpolation is performed
     * @param elem   tetrahedron around which the interpolation is performed */
    double interp_scalar(const Point3 &point, const int elem) const;

    /** Locate the tetrahedron that surrounds or is closest to the point of interest.
     * The search starts from the elem_guess-th tetrahedron, proceedes with its neighbours
     * (number of neighbouring layers is specified inside the function),
     * then, if no match found, checks sequentially all the tetrahedra and if still no match found,
     * returns the index of tetrahedron whose centroid is closest to the point.
     * @param point       point of interest
     * @param elem_guess  index of tetrahedron around which the search starts
     * @return index of the tetrahedron that surrounds or is closest to the point
     */
    int locate_element(const Point3 &point, const int elem_guess);

    /** Print statistics about solution on node points */
    void print_statistics() const;

    /** Print the deviation from the analytical solution of hemiellipsoid on the infinite surface */
    void print_error(const Coarseners& c) const;

    /** Compare the analytical and calculated field enhancement */
    void print_enhancement() const;

    /** Set parameters to calculate analytical solution */
    void set_analyt(const Point3& origin, const double E0, const double radius1, const double radius2=-1);

private:
    double radius1;  ///< Minor semi-axis of ellipse
    double radius2;  ///< Major semi-axis of ellipse
    double E0;       ///< Long-range electric field strength
    Point3 origin;
    
    const TetgenCells<4>* cells() const { return &mesh->elems; }
    const TetgenElements* elems;
    
    vector<SimpleElement> tetrahedra;   ///< tetrahedra node indices

    vector<double> det0;                ///< major determinant for calculating bcc-s
    vector<Vec4> det1;                  ///< minor determinants for calculating 1st bcc
    vector<Vec4> det2;                  ///< minor determinants for calculating 2nd bcc
    vector<Vec4> det3;                  ///< minor determinants for calculating 3rd bcc
    vector<Vec4> det4;                  ///< minor determinants for calculating 4th bcc
    vector<bool> tet_not_valid;         ///< co-planarities of tetrahedra

    /** Pre-compute data about tetrahedra to make interpolation faster */
    void precompute();

    /** Return the mapping between tetrahedral and hexahedral meshes;
     * -1 indicates that mapping for corresponding object was not found */
    void get_maps(vector<int>& tet2hex, vector<int>& cell_indxs, vector<int>& vert_indxs,
            dealii::Triangulation<3>* tria, dealii::DoFHandler<3>* dofh, const double eps);

    /** Force the solution on tetrahedral nodes to be the weighed average
     * of the solutions on its Voronoi cell nodes */
    bool average_tetnodes();

    /** Get barycentric coordinates for a point inside i-th tetrahedron */
    Vec4 get_bcc(const Point3 &point, const int i) const;

    /** Get whether the point is located inside the i-th tetrahedron */
    bool point_in_tetrahedron(const Point3 &point, const int i);

    /** Reserve memory for pre-compute data */
    void reserve_precompute(const int N);

    /** Return the tetrahedron type in vtk format */
    int get_cell_type() const { return 10; }

    /** Return analytical potential value for i-th point near the hemisphere
     * @param radius  radius of the hemisphere
     * @param E0      long range electric field around the hemisphere */
    double get_analyt_potential(const int i, const Point3& origin) const;

    /** Return analytical electric field value for i-th point near the hemisphere
     * @param radius  radius of the hemisphere
     * @param E0      long range electric field around the hemisphere */
    Vec3 get_analyt_field(const int i) const;

    /** Get calculated field enhancement */
    double get_enhancement() const;

    /** Get analytical field enhancement for hemi-ellipsoid on infinite surface */
    double get_analyt_enhancement() const;
};

/** Class to interpolate solution on surface triangles */
class TriangleInterpolator : LinearInterpolator<3> {
public:
    /** Constructor of TriangleInterpolator  */
    TriangleInterpolator(const TetgenMesh* mesh);

    /** Precompute the data needed to calculate the distance of points from surface
     * in the direction of triangle surface norms */
    void precompute();

    /** Find the triangle which contains the point or is the closest to it */
    int locate_face(const Vec3 &point, const int face_guess);

    /** Calculate barycentric coordinates for a projection of a point inside the triangle */
    Vec3 get_bcc(const Vec3& point, const int face) const;

    /** Check whether the projection of a point is inside the triangle */
    bool projection_in_triangle(const Vec3& point, const int face);

    /** Interpolate both vector and scalar data. Function assumes, that triangle,
     * that surrounds the point projection, is previously already found with locate_element. */
    Solution interp_solution(const Point3 &point, const int face) const;

    /** Interpolate vector data. Function assumes, that triangle,
     * that surrounds the point projection, is previously already found with locate_element. */
    Vec3 interp_vector(const Point3 &point, const int face) const;

    /** Interpolate scalar data. Function assumes, that triangle,
     * that surrounds the point projection, is previously already found with locate_element. */
    double interp_scalar(const Point3 &point, const int face) const;

private:
    /** Data computed before starting looping through the triangles */
    vector<Vec3> vert0;
    vector<Vec3> edge1;
    vector<Vec3> edge2;
    vector<Vec3> pvec;
    vector<bool> is_parallel;

    vector<SimpleFace> triangles;
    
    const TetgenCells<3>* cells() const { return &mesh->faces; }
    const TetgenFaces* faces;

    /** Reserve memory for precompute data */
    void reserve_precompute(const int n);

    /** Return the triangle type in vtk format */
    int get_cell_type() const { return 5; }
};

} // namespace femocs

#endif /* LINEARINTERPOLATOR_H_ */
