/*
 * DealII.cpp
 *  Created on: 11.2.2016
 *      Author: Mihkel Veske, Kristjan Eimre
 */

#include "DealII.h"
#include <fstream>

#include <deal.II/grid/grid_in.h>
#include <deal.II/grid/grid_out.h>
#include <deal.II/grid/grid_tools.h>
#include <deal.II/grid/grid_reordering.h>
#include <deal.II/numerics/matrix_tools.h>
#include <deal.II/lac/precondition.h>
#include <deal.II/numerics/vector_tools.h>
#include <deal.II/dofs/dof_tools.h>
#include <numeric>

using namespace std;
using namespace dealii;

namespace femocs {

// Laplace solver constructor
// Number in fe() determines the interpolation type; 1 is linear etc.
DealII::DealII() :
        dof_handler(triangulation), neumann(0), fe(POLY_DEGREE) {}

// Specify the Neumann boundary condition value
void DealII::set_applied_efield(const double elfield) {
    this->neumann = elfield;
}

// Import mesh from file
bool DealII::import_mesh(const string &file_name) {
    const string file_type = get_file_type(file_name);
    require(file_type == "vtk" || file_type == "msh", "Unknown file type: " + file_type);

    GridIn<DIM, DIM> gi;
    gi.attach_triangulation(triangulation);
    ifstream in_file(file_name);
    require(in_file, "File " + file_name + " could not be opened!");

    if (file_type == "vtk")
        gi.read_vtk(in_file);
    else if (file_type == "msh") gi.read_msh(in_file);

    return 0;
}

// Import vertices and hexahedra and ignore quadrangles
// It's preferred function because Deal.II makes the faces together with all the other stuff it needs itself anyways
bool DealII::import_mesh(vector<Point<DIM>> vertices, vector<CellData<DIM>> cells) {
    try {
        SubCellData subcelldata;               // empty array for faces
        // Do some clean-up on vertices...
        GridTools::delete_unused_vertices(vertices, cells, subcelldata);
        // ... and on cells
        GridReordering<DIM, DIM>::invert_all_cells_of_negative_grid(vertices, cells);
//        GridReordering<DIM, DIM>::reorder_cells(cells);

        triangulation.create_triangulation_compatibility(vertices, cells, SubCellData());
    } catch (exception &exc) {
        return 1;
    }
    return 0;
}

// Make mesh 4x denser around origin
void DealII::refine_mesh(const Point<DIM> &origin, const double radius) {

    typename Triangulation<DIM>::active_cell_iterator cell;
    for (cell = triangulation.begin_active(); cell != triangulation.end(); ++cell)
        if (origin.distance(cell->center()) < radius) cell->set_refine_flag();

    triangulation.execute_coarsening_and_refinement();
}

// Mark the boundary faces of mesh
void DealII::mark_boundary_faces(const Medium::Sizes& sizes) {
    const double eps = 0.1;
    typename Triangulation<DIM>::active_face_iterator face;

    // Loop through the faces and mark them according to the location of their centre
    for (face = triangulation.begin_face(); face != triangulation.end_face(); ++face)
        if (face->at_boundary()) {
            if (on_boundary(face->center()[0], sizes.xmin, sizes.xmax, eps))
                face->set_all_boundary_ids(TYPES.PERIMETER);
            else if (on_boundary(face->center()[1], sizes.ymin, sizes.ymax, eps))
                face->set_all_boundary_ids(TYPES.PERIMETER);
            else if (on_boundary(face->center()[2], sizes.zmaxbox, eps))
                face->set_all_boundary_ids(TYPES.ZMAX);
            else
                face->set_all_boundary_ids(TYPES.SURFACE);
        }
}

Triangulation<DIM>* DealII::get_triangulation() {
    return &triangulation;
}

DoFHandler<DIM>* DealII::get_dof_handler() {
    return &dof_handler;
}

// Mark boundary faces, distribute degrees of freedom and initialise data
void DealII::setup_system(const Medium::Sizes& sizes) {
    mark_boundary_faces(sizes);

    dof_handler.distribute_dofs(fe);

    DynamicSparsityPattern dsp(dof_handler.n_dofs());
    DoFTools::make_sparsity_pattern(dof_handler, dsp);
    sparsity_pattern.copy_from(dsp);

    system_matrix.reinit(sparsity_pattern);
    system_rhs.reinit(dof_handler.n_dofs());
    laplace_solution.reinit(dof_handler.n_dofs());
}

// Insert boundary conditions to the system
void DealII::assemble_system() {
    // Set up quadrature system for quads and faces
    const QGauss<DIM> quadrature_formula(2);
    const QGauss<DIM - 1> face_quadrature_formula(2);
    unsigned int i, j, q_index;

    // Calculate necessary values (derived from weak form of Laplace equation)
    FEValues<DIM> fe_values(fe, quadrature_formula,
            update_values | update_gradients | update_quadrature_points | update_JxW_values);
    FEFaceValues<DIM> fe_face_values(fe, face_quadrature_formula,
            update_values | update_gradients | update_quadrature_points | update_JxW_values);

    // Parametrize necessary entities.
    const unsigned int n_dofs_per_cell = fe.dofs_per_cell;
    const unsigned int n_faces_per_elem = GeometryInfo<3>::faces_per_cell;
//    const double space_charge = 0.0;

    // Declare cell matrix and right-hand-side matrix (sets coordinate system so we get real positive cells)
    FullMatrix<double> cell_matrix(n_dofs_per_cell, n_dofs_per_cell);
    Vector<double> cell_rhs(n_dofs_per_cell);

    // Create a vector of local degrees of freedom for each cell.
    vector<types::global_dof_index> local_dof_indices(n_dofs_per_cell);

    //Start iterator cycles over all cells to set local terms for each cell
    DoFHandler<DIM>::active_cell_iterator cell;
    for (cell = dof_handler.begin_active(); cell != dof_handler.end(); ++cell) {
        fe_values.reinit(cell);
        cell_matrix = 0;
        cell_rhs = 0;

        // Integration of the cell by looping over all quadrature points
        for (q_index = 0; q_index < quadrature_formula.size(); ++q_index) {
            // Assemble the right hand side by integrating over the shape function i times the
            // right hand side function; we choose it to be the function with constant value one
//            for (i = 0; i < n_dofs_per_cell; ++i)
//                cell_rhs(i) += fe_values.shape_value(i, q_index) * space_charge
//                    * fe_values.JxW(q_index);

            // Assemble the matrix
            for (i = 0; i < n_dofs_per_cell; ++i)
                for (j = 0; j < n_dofs_per_cell; ++j)
                    cell_matrix(i, j) += fe_values.shape_grad(i, q_index) * fe_values.JxW(q_index)
                            * fe_values.shape_grad(j, q_index);
        }

        // Cycle for faces of each cell
        for (unsigned int f = 0; f < n_faces_per_elem; ++f) {
            // Check if face is located at top boundary
            if (cell->face(f)->boundary_id() == TYPES.ZMAX) {
                fe_face_values.reinit(cell, f);

                // Apply Neumann boundary condition
                for (q_index = 0; q_index < face_quadrature_formula.size(); ++q_index)
                    for (i = 0; i < n_dofs_per_cell; ++i)
                        cell_rhs(i) += fe_face_values.shape_value(i, q_index) * neumann
                                * fe_face_values.JxW(q_index);
            }
        }

        // Apply set conditions and rewrite system matrix & rhs
        cell->get_dof_indices(local_dof_indices);
        for (i = 0; i < n_dofs_per_cell; ++i)
            for (j = 0; j < n_dofs_per_cell; ++j)
                system_matrix.add(local_dof_indices[i], local_dof_indices[j], cell_matrix(i, j));

        for (i = 0; i < n_dofs_per_cell; ++i)
            system_rhs(local_dof_indices[i]) += cell_rhs(i);

    }

    // Declare boundaries
    map<types::global_dof_index, double> bv;

    // Add Dirichlet' boundary condition to faces denoted as surface
    VectorTools::interpolate_boundary_values(dof_handler, TYPES.SURFACE, ZeroFunction<DIM>(), bv);

    // Apply boundary values to system matrix
    MatrixTools::apply_boundary_values(bv, system_matrix, laplace_solution, system_rhs);
}

// Run the calculation with Conjugate Gradient solver
void DealII::solve_cg() {
    const int n_steps = 10000;      // max number of iterations
    const double tolerance = 1e-9;  // solver tolerance
    SolverControl solver_control(n_steps, tolerance);
    SolverCG<> solver(solver_control);
    solver.solve(system_matrix, laplace_solution, system_rhs, PreconditionIdentity());
}

// Run the calculation with UMFPACK solver
void DealII::solve_umfpack() {
    SparseDirectUMFPACK A_direct;
    A_direct.initialize(system_matrix);
    A_direct.vmult(laplace_solution, system_rhs);
}

// Calculate electric field at arbitrary point inside the mesh
Vec3 DealII::get_efield(const double x, const double y, const double z) {
    Tensor<1, DIM> ef = -1.0 * VectorTools::point_gradient(dof_handler, laplace_solution, Point<DIM>(x, y, z));
    return Vec3(ef[0], ef[1], ef[2]);
}

// Calculate electric field at a set of mesh nodes
vector<Tensor<1, DIM>> DealII::get_efield(const vector<int> &cell_indxs, const vector<int> &vert_indxs) {
    const int n_cells = cell_indxs.size();
    // Initialise potentials with a value that is immediately visible if it's not changed to proper one
    vector<Tensor<1, DIM>> elfields(n_cells);

    vector<Tensor<1, DIM>> solution_gradients;
    QTrapez<DIM> only_vertices_quadrature_formula;
    FEValues<DIM> fe_values(fe, only_vertices_quadrature_formula, update_gradients);
    solution_gradients.resize(only_vertices_quadrature_formula.size());

    // Generate sort indices for cell_indxs so that elements could be accessed sequentially
    vector<int> sort_indxs = get_sort_indices(cell_indxs, "up");

    typename DoFHandler<DIM>::active_cell_iterator cell = dof_handler.begin_active();

    // Iterate through all the cells and get the electric field from ones listed in cell_indxs
    for (int i = 0; (i < n_cells) && (cell != dof_handler.end()); ) {
        int si = sort_indxs[i];
        if ((int)cell->active_cell_index() == cell_indxs[si]) {
            require(vert_indxs[si] >= 0 && vert_indxs[si] < n_verts_per_elem, "Invalid index: " + to_string(vert_indxs[si]));
            fe_values.reinit(cell);
            fe_values.get_function_gradients(laplace_solution, solution_gradients);
            elfields[si] = -1.0 * solution_gradients.at(vert_indxs[si]);

            ++i;
        }
        else ++cell;
    }
    return elfields;
}

// Calculate potential at arbitrary point inside the mesh
double DealII::get_potential(const double x, const double y, const double z) {
    return VectorTools::point_value(dof_handler, laplace_solution, Point<DIM>(x, y, z));
}

// Get potential at a set of mesh nodes
vector<double> DealII::get_potential(const vector<int> &cell_indxs, const vector<int> &vert_indxs) {
    const int n_cells = cell_indxs.size();

    // Initialise potentials with a value that is immediately visible if it's not changed to proper one
    vector<double> potentials(n_cells, 1e15);

    // Generate sort indices for cell_indxs so that elements could be accessed sequentially
    vector<int> sort_indxs = get_sort_indices(cell_indxs, "up");

    typename DoFHandler<DIM>::active_cell_iterator cell = dof_handler.begin_active();

    // Iterate through all the cells and get the potential from the ones listed in cell_indxs
    for (int i = 0; (i < n_cells) && (cell != dof_handler.end()); ) {
        int si = sort_indxs[i];
        if ((int)cell->active_cell_index() == cell_indxs[si]) {
            require(vert_indxs[si] >= 0 && vert_indxs[si] < n_verts_per_elem, "Invalid index: " + to_string(vert_indxs[si]));
            potentials[si] = laplace_solution( cell->vertex_dof_index(vert_indxs[si], 0) );
             ++i;
        }
        else ++cell;
    }

    return potentials;
}

// Write the potential and electric field to the file
void DealII::write(const string &file_name) {
    if (!MODES.WRITEFILE) return;

    string ftype = get_file_type(file_name);
    require(ftype == "vtk" || ftype == "eps", "Unsupported file type: " + ftype);

    LaplacePostProcessor field_calculator("Electric_field");
    DataOut<DIM> data_out;
    data_out.attach_dof_handler(dof_handler);
    data_out.add_data_vector(laplace_solution, "Potential");
    data_out.add_data_vector(laplace_solution, field_calculator);
    data_out.build_patches();

    ofstream outfile(file_name);
    require(outfile, "File " + file_name + " cannot be opened for writing!");
    if (ftype == "vtk")
        data_out.write_vtk(outfile);
    else if (ftype == "eps") data_out.write_eps(outfile);
}

// Write the mesh to the file
void DealII::write_mesh(const string &file_name) {
    if (!MODES.WRITEFILE) return;

    string ftype = get_file_type(file_name);
    require(ftype == "vtk" || ftype == "msh" || ftype == "eps", "Unsupported file type: " + ftype);

    ofstream outfile(file_name);
    require(outfile, "File " + file_name + " cannot be opened for writing!");
    GridOut gout;

    if (ftype == "vtk")
        gout.write_vtk(triangulation, outfile);
    else if (ftype == "msh")
        gout.write_msh(triangulation, outfile);
    else if (ftype == "eps") gout.write_eps(triangulation, outfile);
}

} /* namespace femocs */