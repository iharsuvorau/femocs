/*
 * Femocs.cpp
 *
 *  Created on: 14.12.2015
 *      Author: veske
 */

#include <omp.h>
#include "Femocs.h"

#include "Coarseners.h"
#include "DealII.h"
#include "Macros.h"
#include "Medium.h"
#include "Media.h"
#include "Tethex.h"
#include "TetgenMesh.h"

using namespace std;
namespace femocs {

// specify simulation parameters
Femocs::Femocs(string path_to_conf) : skip_calculations(false) {
    double t0;
    start_msg(t0, "======= Femocs started! =======\n");

    start_msg(t0, "=== Reading configuration parameters...");
    conf.read_all(path_to_conf);
    end_msg(t0);
    conf.print_data();

#if HEATINGMODE
        start_msg(t0, "=== Loading physical quantities...");
        string pq_path = "heating/res/physical_quantities/";
        phys_quantities.load_emission_data(pq_path + "gtf_grid_1000x1000.dat");
        phys_quantities.load_nottingham_data(pq_path + "nottingham_grid_300x300.dat");
        phys_quantities.load_resistivity_data(pq_path + "cu_res.dat");
        ch_solver1.set_pq(phys_quantities);
        ch_solver2.set_pq(phys_quantities);
        end_msg(t0);

        fch_solver  = &ch_solver1;
        prev_fch_solver = NULL;
        even_run = true;
#endif

    // Create the output folder if file writing is enabled and it doesn't exist
    if (MODES.WRITEFILE) system("mkdir -p output");
}

// delete data and print bye-bye-message
Femocs::~Femocs() {
    start_msg(double t0, "======= Femocs finished! =======\n");
}

// workhorse function to generate FEM mesh and to solve differential equation(s)
const int Femocs::run(double elfield, string message) {
    double t0, tstart;  // Variables used to measure the code execution time
    bool success;

    if(skip_calculations) return skip_calculations;
    skip_calculations = true;

    conf.neumann = elfield;
    conf.message = message;
    tstart = omp_get_wtime();

    // ====================================
    // ===== Converting imported data =====
    // ====================================

    start_msg(t0, "=== Extracting surface...");
    Surface dense_surf;
    dense_surf.extract(&reader);
    dense_surf.write("output/surface_dense.xyz");
    end_msg(t0);

    start_msg(t0, "=== Coarsening surface...");
    Coarseners coarseners;
    coarseners.generate(dense_surf, conf.radius, conf.cfactor, conf.latconst);

    Surface coarse_surf;
    coarse_surf = dense_surf.coarsen(coarseners, &reader.sizes);
    coarse_surf.write("output/surface_coarse.xyz");
    end_msg(t0);

    start_msg(t0, "=== Smoothing surface...");
    coarse_surf.smoothen(conf.radius, conf.smooth_factor, 2.0*conf.coord_cutoff);
    coarse_surf.write("output/surface_smooth.xyz");
    end_msg(t0);

    start_msg(t0, "=== Resizing simulation box...");
    coarse_surf.calc_statistics();  // calculate zmin and zmax for surface
    reader.resize_box(coarse_surf.sizes.zmin - conf.zbox_below, coarse_surf.sizes.zmax + conf.zbox_above);
    end_msg(t0);

    start_msg(t0, "=== Generating bulk...");
    Bulk bulk;
    bulk.generate_simple(&reader.sizes);
    bulk.write("output/bulk.xyz");
    end_msg(t0);

    start_msg(t0, "=== Generating vacuum...");
    Vacuum vacuum;
    vacuum.generate_simple(&reader.sizes);
    vacuum.write("output/vacuum.xyz");
    end_msg(t0);

    // ===========================
    // ===== Making FEM mesh =====
    // ===========================

    start_msg(t0, "=== Making big mesh...");
    TetgenMesh tetmesh_big;
    // r - reconstruct, n - output neighbour list, Q - quiet, q - mesh quality
    success = tetmesh_big.generate(bulk, coarse_surf, vacuum, "rnQq" + conf.mesh_quality);
    check_message(!success, "Triangulation failed! Field calculation will be skipped!");

    tetmesh_big.nodes.write("output/nodes_generated.xyz");
    tetmesh_big.faces.write("output/faces_generated.vtk");
    tetmesh_big.elems.write("output/elems_generated.vtk");
    end_msg(t0);

    start_msg(t0, "=== Making surface faces...");
    tetmesh_big.generate_appendices();
    tetmesh_big.faces.write("output/faces_appended.vtk");
    end_msg(t0);

    start_msg(t0, "=== Marking tetrahedral mesh...");
    success = tetmesh_big.mark_mesh(conf.postprocess_marking);
    tetmesh_big.nodes.write("output/nodes_marked.xyz");
    tetmesh_big.faces.write("output/faces_marked.vtk");
    tetmesh_big.elems.write("output/elems_marked.vtk");
    check_message(!success, "Mesh marking failed! Field calcualtion will be skipped!");
    end_msg(t0);

    start_msg(t0, "=== Converting tetrahedra to hexahedra...");
    tethex::Mesh hexmesh_big;
    hexmesh_big.read_femocs(tetmesh_big);
    hexmesh_big.convert();
    end_msg(t0);

    start_msg(t0, "=== Smoothing hexahedra...");
    hexmesh_big.smoothen(conf.radius, conf.smooth_factor, 1.5*conf.coord_cutoff);
    end_msg(t0);

    start_msg(t0, "=== Separating tetrahedral meshes...");
    TetgenMesh tetmesh_vacuum;
    TetgenMesh tetmesh_bulk;
    hexmesh_big.export_vertices(tetmesh_big);  // correcting the nodes in tetrahedral mesh
    tetmesh_big.separate_meshes(tetmesh_bulk, tetmesh_vacuum, "rnQ");

    tetmesh_bulk.nodes.write("output/nodes_bulk.xyz");
    tetmesh_bulk.faces.write("output/faces_bulk.vtk");
    tetmesh_bulk.elems.write("output/elems_bulk.vtk");

    tetmesh_vacuum.nodes.write("output/nodes_vacuum.xyz");
    tetmesh_vacuum.faces.write("output/faces_vacuum.vtk");
    tetmesh_vacuum.elems.write("output/elems_vacuum.vtk");
    end_msg(t0);
    
    start_msg(t0, "=== Separating hexahedral meshes...");
    tethex::Mesh hexmesh_bulk;
    tethex::Mesh hexmesh_vacuum;
    hexmesh_big.separate_meshes(hexmesh_bulk, hexmesh_vacuum);
    end_msg(t0);

    // ====================================
    // ===== Running FEM multi-solver =====
    // ====================================

#if HEATINGMODE
        start_msg(t0, "=== Running Laplace solver...");
        fch::Laplace<3> laplace_solver;
        laplace_solver.import_mesh_directly(hexmesh_vacuum.get_nodes(), hexmesh_vacuum.get_elems());
        laplace_solver.setup_system();
        laplace_solver.assemble_system();
        laplace_solver.solve();
        laplace_solver.output_results("output/laplace_solution.vtk");
        end_msg(t0);

        start_msg(t0, "=== Calculating current density and T...\n");
        fch_solver->reinitialize(&laplace_solver, prev_fch_solver);
        fch_solver->import_mesh_directly(hexmesh_bulk.get_nodes(), hexmesh_bulk.get_elems());
        double temp_error = fch_solver->run_specific(10.0, 10, MODES.WRITEFILE, "output/fch", MODES.VERBOSE);
        end_msg(t0);

        if (even_run) {
            fch_solver = &ch_solver2;
            prev_fch_solver = &ch_solver1;
            even_run = false;
        } else {
            fch_solver = &ch_solver1;
            prev_fch_solver = &ch_solver2;
            even_run = true;
        }
#endif

    // ==============================
    // ===== Running FEM solver =====
    // ==============================

    DealII laplace;
    laplace.set_neumann(conf.neumann);

    start_msg(t0, "=== Importing mesh into Deal.II...");
    success = laplace.import_mesh_wo_faces(hexmesh_vacuum);
    check_message(!success, "Importing mesh to Deal.II failed! Field calculation will be skipped!");
    end_msg(t0);

    if (conf.refine_apex) {
        start_msg(t0, "=== Refining mesh in Deal.II...");
        Point3 origin(coarse_surf.sizes.xmid, coarse_surf.sizes.ymid, coarse_surf.sizes.zmax);
        laplace.refine_mesh(origin, 7*conf.latconst);
        laplace.write_mesh("output/elems_dealii.vtk");
        end_msg(t0);
    }

    start_msg(t0, "=== System setup...");
    laplace.setup_system(reader.sizes);
    end_msg(t0);

    if(MODES.VERBOSE) {
        cout << "#input atoms: " << reader.get_n_atoms() << endl;
        cout << "Vacuum:  " << tetmesh_vacuum << endl;
        cout << "Bulk:    " << tetmesh_bulk << endl;
        cout << "Laplace: " << laplace << endl;
    }

    start_msg(t0, "=== Assembling system...");
    laplace.assemble_system();
    end_msg(t0);

    start_msg(t0, "=== Solving...");
    laplace.solve_cg();
//    laplace.solve_umfpack();
    end_msg(t0);

    // =======================================================
    // ===== Extracting and post-processing FEM solution =====
    // =======================================================

    start_msg(t0, "=== Extracting solution...");
    solution.extract_solution(laplace, tetmesh_vacuum.nodes);
    end_msg(t0);

    start_msg(t0, "=== Pre-computing interpolator...");
    interpolator.precompute_tetrahedra(tetmesh_vacuum);
    small_interpolator.precompute_tetrahedra(tetmesh_vacuum);
    end_msg(t0);

    start_msg(t0, "=== Interpolating solution...");
    dense_surf.sort_atoms(0, 1, "up");
    interpolator.extract_interpolation(dense_surf);
    end_msg(t0);

    start_msg(t0, "=== Cleaning interpolation...");
    interpolator.clean(0, conf.n_bins, conf.smooth_factor, 3*conf.coord_cutoff);
    interpolator.clean(1, conf.n_bins, conf.smooth_factor, 3*conf.coord_cutoff);
    interpolator.clean(2, conf.n_bins, conf.smooth_factor, 3*conf.coord_cutoff);
    interpolator.clean(3, conf.n_bins, conf.smooth_factor, 3*conf.coord_cutoff);
    interpolator.clean(4, conf.n_bins, conf.smooth_factor, 3*conf.coord_cutoff);
    end_msg(t0);

    start_msg(t0, "=== Saving results...");
    reader.save_current_run_points(conf.distance_tol);
//    laplace.write("output/result.vtk");
    solution.write("output/result.xyz");
    interpolator.write("output/interpolation" + conf.message + ".xyz");

    coarseners.write("output/coarseners" + message + ".vtk");
    hexmesh_bulk.write_vtk_elems("output/bulk_smooth" + message + ".vtk");
    hexmesh_vacuum.write_vtk_elems("output/vacuum_smooth" + message + ".vtk");
    end_msg(t0);

    cout << "\nTotal time of Femocs.run: " << omp_get_wtime() - tstart << "\n";
    skip_calculations = false;

    return skip_calculations;
}

// import atoms from file
const int Femocs::import_atoms(const string& file_name) {
    double t0;
    string file_type, fname;

    if (file_name == "") fname = conf.infile;
    else fname = file_name;

    file_type = get_file_type(fname);
    expect(file_type == "ckx" || file_type == "xyz", "Unknown file type: " + file_type);

    start_msg(t0, "=== Importing atoms...");
    reader.import_file(fname);
    end_msg(t0);

    start_msg(t0, "=== Comparing with previous run...");
    skip_calculations = reader.equals_previous_run(conf.distance_tol);
    end_msg(t0);

    check_message(skip_calculations, "Atoms haven't moved significantly! Field calculation will be skipped!");

    if (file_type == "xyz") {
        start_msg(t0, "=== Calculating coords and atom types...");
        reader.calc_coordination(conf.coord_cutoff);
        reader.extract_types(conf.nnn, conf.latconst);
        end_msg(t0);
    } else {
        start_msg(t0, "=== Calculating coords from atom types...");
        reader.calc_coordination(conf.nnn);
        end_msg(t0);
    }

    return skip_calculations;
}

// import atoms from PARCAS
const int Femocs::import_atoms(int n_atoms, double* coordinates, double* box, int* nborlist) {
    double t0;

    start_msg(t0, "=== Importing atoms...");
    reader.import_parcas(n_atoms, coordinates, box);
    end_msg(t0);

    start_msg(t0, "=== Comparing with previous run...");
    skip_calculations = reader.equals_previous_run(conf.distance_tol);
    end_msg(t0);

    check_message(skip_calculations, "Atoms haven't moved significantly! Field calculation will be skipped!");

    start_msg(t0, "=== Calculating coords and atom types...");
    reader.calc_coordination(conf.nnn, conf.coord_cutoff, nborlist);
    reader.extract_types(conf.nnn, conf.latconst);
    end_msg(t0);

    return skip_calculations;
}

// import coordinates and types of atoms
const int Femocs::import_atoms(int n_atoms, double* x, double* y, double* z, int* types) {
    double t0;

    start_msg(t0, "=== Importing atoms...");
    reader.import_helmod(n_atoms, x, y, z, types);
    end_msg(t0);

    start_msg(t0, "=== Comparing with previous run...");
    skip_calculations = reader.equals_previous_run(conf.distance_tol);
    end_msg(t0);

    check_message(skip_calculations, "Atoms haven't moved significantly! Field calculation will be skipped!");

    start_msg(t0, "=== Calculating coords from atom types...");
    reader.calc_coordination(conf.nnn);
    end_msg(t0);

    return skip_calculations;
}

// export the calculated electric field on imported atom coordinates
const int Femocs::export_elfield(int n_atoms, double* Ex, double* Ey, double* Ez, double* Enorm) {
    double t0;
    start_msg(t0, "=== Exporting results...");
    interpolator.export_interpolation(n_atoms, Ex, Ey, Ez, Enorm);
    end_msg(t0);
    return skip_calculations;
}

// linearly interpolate electric field at given points
const int Femocs::interpolate_elfield(int n_points, double* x, double* y, double* z,
        double* Ex, double* Ey, double* Ez, double* Enorm, int* flag) {

    small_interpolator.extract_elfield(n_points, x, y, z, Ex, Ey, Ez, Enorm, flag);
    small_interpolator.write("output/elfield_on_points.xyz");
    return skip_calculations;
}

// linearly interpolate electric potential at given points
const int Femocs::interpolate_phi(int n_points, double* x, double* y, double* z, double* phi, int* flag) {
    small_interpolator.extract_potential(n_points, x, y, z, phi, flag);
    small_interpolator.write("output/phi_on_points.xyz");
    return skip_calculations;
}

// parse integer argument of the command from input script
const int Femocs::parse_command(const string& command, int* arg) {
    return conf.read_command(command, arg[0]);
}

// parse double argument of the command from input script
const int Femocs::parse_command(const string& command, double* arg) {
    return conf.read_command(command, arg[0]);
}

} // namespace femocs
