/*
 * Femocs.cpp
 *
 *  Created on: 14.12.2015
 *      Author: veske
 */

#include "Femocs.h"
#include "Coarseners.h"
#include "Macros.h"
#include "Tethex.h"
#include "TetgenMesh.h"

#include <omp.h>

using namespace std;
namespace femocs {

// specify simulation parameters
Femocs::Femocs(string path_to_conf) : skip_calculations(false) {
    start_msg(t0, "======= Femocs started! =======\n");

    start_msg(t0, "=== Reading configuration parameters...");
    conf.read_all(path_to_conf);
    end_msg(t0);
    conf.print_data();

    start_msg(t0, "=== Reading physical quantities...");
    ch_solver1.set_physical_quantities(&phys_quantities);
    ch_solver2.set_physical_quantities(&phys_quantities);
    ch_solver  = &ch_solver1;
    prev_ch_solver = NULL;
    end_msg(t0);

    // Clear the results from previous run
    if (conf.clear_output) system("rm -rf output");
    if (MODES.WRITEFILE) system("mkdir -p output");
}

// delete data and print bye-bye-message
Femocs::~Femocs() {
    start_msg(t0, "======= Femocs finished! =======\n");
}

// Generate boundary nodes for mesh
const int Femocs::generate_boundary_nodes(Media& bulk, Media& coarse_surf, Media& vacuum) {
    start_msg(t0, "=== Extracting surface...");
    dense_surf.extract(reader, TYPES.SURFACE);
    dense_surf = dense_surf.clean_lonely_atoms(conf.coord_cutoff);
    end_msg(t0);
    dense_surf.write("output/surface_dense.xyz");

    Coarseners coarseners;
    coarseners.generate(dense_surf, conf.radius, conf.cfactor, conf.latconst);
    coarseners.write("output/coarseners.vtk");
    
    start_msg(t0, "=== Coarsening & stretching surface...");
    Media stretch_surf;
    stretch_surf = dense_surf.stretch(conf.latconst, conf.box_width);
    coarse_surf = stretch_surf.coarsen(coarseners, stretch_surf.sizes);
    end_msg(t0);
    coarse_surf.write("output/surface_nosmooth.xyz");
    stretch_surf.write("output/surface_stretch.xyz");

    start_msg(t0, "=== Smoothing surface...");
    coarse_surf.smoothen(conf.radius, conf.smooth_factor, 3.0*conf.coord_cutoff);
    end_msg(t0);
    coarse_surf.write("output/surface_coarse.xyz");
    
    start_msg(t0, "=== Generating bulk & vacuum...");
    coarse_surf.calc_statistics();  // calculate zmin and zmax for surface
    bulk.generate_simple(coarse_surf.sizes, coarse_surf.sizes.zmin - conf.bulk_height * conf.latconst);
    vacuum.generate_simple(coarse_surf.sizes, coarse_surf.sizes.zmin + conf.box_height * coarse_surf.sizes.zbox);
    reader.resize_box(coarse_surf.sizes.xmin, coarse_surf.sizes.xmax, 
        coarse_surf.sizes.ymin, coarse_surf.sizes.ymax,
        bulk.sizes.zmin, vacuum.sizes.zmax);
    end_msg(t0);
    
    bulk.write("output/bulk.xyz");
    vacuum.write("output/vacuum.xyz");
    
    return 0;
}

// Generate bulk and vacuum meshes
const int Femocs::generate_meshes(TetgenMesh& bulk_mesh, TetgenMesh& vacuum_mesh) {
    bool fail;

    Media bulk, coarse_surf, vacuum;
    fail = generate_boundary_nodes(bulk, coarse_surf, vacuum);
    if (fail) return 1;
    
    start_msg(t0, "=== Making big mesh...");
    TetgenMesh big_mesh;
    // r - reconstruct, n - output neighbour list, Q - quiet, q - mesh quality
    fail = big_mesh.generate(bulk, coarse_surf, vacuum, "rnQq" + conf.mesh_quality);
    check_message(fail, "Triangulation failed! Field calculation will be skipped!");
    end_msg(t0);

    start_msg(t0, "=== Making surface faces...");
    big_mesh.generate_appendices();
    end_msg(t0);

    big_mesh.faces.write("output/surface_faces.vtk");

    start_msg(t0, "=== Marking tetrahedral mesh...");
    fail = big_mesh.mark_mesh(conf.postprocess_marking);
    big_mesh.nodes.write("output/tetmesh_nodes.xyz");
    big_mesh.nodes.write("output/tetmesh_nodes.vtk");
    big_mesh.elems.write("output/tetmesh_elems.vtk");
    check_message(fail, "Mesh marking failed! Field calcualtion will be skipped!");
    end_msg(t0);

    start_msg(t0, "=== Converting tetrahedra to hexahedra...");
    big_mesh.generate_hexs();
    end_msg(t0);
    big_mesh.nodes.write("output/hexmesh_nodes.vtk");
    big_mesh.hexahedra.write("output/hexmesh_elems.vtk");

    start_msg(t0, "=== Smoothing hexahedra...");
    big_mesh.smoothen(conf.radius, conf.smooth_factor, 3.0*conf.coord_cutoff);
    end_msg(t0);

    start_msg(t0, "=== Separating vacuum & bulk meshes...");
    big_mesh.separate_meshes(bulk_mesh, vacuum_mesh, "rnQ");
    end_msg(t0);

    expect(bulk_mesh.nodes.size()>0, "Zero nodes in bulk mesh!");
    expect(vacuum_mesh.nodes.size()>0, "Zero nodes in vacuum mesh!");
    expect(bulk_mesh.hexahedra.size()>0, "Zero elements in bulk mesh!");
    expect(vacuum_mesh.hexahedra.size()>0, "Zero elements in vacuum mesh!");

    if (MODES.VERBOSE)
        cout << "Bulk:   " << bulk_mesh << "\nVacuum: " << vacuum_mesh << endl;

    return 0;
}

// Solve Laplace equation
const int Femocs::solve_laplace(TetgenMesh& mesh, fch::Laplace<3>& solver) {
    bool fail;

    start_msg(t0, "=== Importing mesh to Laplace solver...");
    fail = !solver.import_mesh_directly(mesh.nodes.export_dealii(), mesh.hexahedra.export_dealii());
    check_message(fail, "Importing mesh to Deal.II failed! Field calculation will be skipped!");
    end_msg(t0);

//    if (conf.refine_apex) {
//        start_msg(t0, "=== Refining mesh in Laplace solver...");
//        Point3 origin(dense_surf.sizes.xmid, dense_surf.sizes.ymid, dense_surf.sizes.zmax);
//        laplace.refine_mesh(origin, 7*conf.latconst);
//        laplace.write_mesh("output/hexmesh_refine.vtk");
//        end_msg(t0);
//    }

    start_msg(t0, "=== Initializing Laplace solver...");
    solver.set_applied_efield(10.0*conf.neumann);
    solver.setup_system();
    solver.assemble_system();
    end_msg(t0);

    if (MODES.VERBOSE) cout << solver << endl;

    start_msg(t0, "=== Running Laplace solver...");
    solver.solve();
    end_msg(t0);

    if (MODES.WRITEFILE) solver.output_results("output/result_E_phi" + conf.message + ".vtk");

    return 0;
}

// Solve heat and continuity equations
const int Femocs::solve_heat(TetgenMesh& mesh, fch::Laplace<3>& laplace_solver) {
    bool fail;

    start_msg(t0, "=== Initializing rho & T solver...");
    ch_solver->reinitialize(&laplace_solver, prev_ch_solver);
    end_msg(t0);

    start_msg(t0, "=== Importing mesh to rho & T solver...");
    fail = !ch_solver->import_mesh_directly(mesh.nodes.export_dealii(), mesh.hexahedra.export_dealii());
    check_message(fail, "Importing mesh to Deal.II failed! rho & T calculation will be skipped!");
    ch_solver->setup_system();
    end_msg(t0);

    if (MODES.VERBOSE) cout << *(ch_solver) << endl;

    start_msg(t0, "=== Running rho & T solver...\n");
    double temp_error = ch_solver->run_specific(conf.t_error, conf.n_newton, false, "", MODES.VERBOSE, 2.0);
    end_msg(t0);

    if (MODES.WRITEFILE) ch_solver->output_results("output/result_rho_T" + conf.message + ".vtk");
    check_message(temp_error > conf.t_error, "Temperature didn't converge, err=" + to_string(temp_error)) + "! Using previous solution!";

    return 0;
}

// Extract electric potential and electric field from solution
const int Femocs::extract_laplace(TetgenMesh& mesh, fch::Laplace<3>* solver) {
    start_msg(t0, "=== Extracting E and phi...");
    vacuum_interpolator.extract_solution(solver, mesh);
    end_msg(t0);

    vacuum_interpolator.write("output/result_E_phi.xyz");

    start_msg(t0, "=== Interpolating E and phi...");
    vacuum_interpolation.interpolate(dense_surf, conf.coord_cutoff*conf.smoothen_solution);
    end_msg(t0);

    vacuum_interpolation.write("output/interpolation_vacuum" + conf.message + ".vtk");

    return 0;
}

// Extract current density and temperature from solution
const int Femocs::extract_heat(TetgenMesh& mesh, fch::CurrentsAndHeating<3>* solver) {
    start_msg(t0, "=== Extracting T and rho...");
    bulk_interpolator.extract_solution(solver, mesh);
    end_msg(t0);

    bulk_interpolator.write("output/result_rho_T.xyz");

    start_msg(t0, "=== Interpolating T and rho...");
    bulk_interpolation.interpolate(reader, 0);
    end_msg(t0);

    bulk_interpolation.write("output/interpolation_bulk" + conf.message + ".vtk");

    return 0;
}

// Workhorse function to generate FEM mesh and to solve differential equation(s)
const int Femocs::run(double elfield, string message) {
    double tstart;  // Variables used to measure the code execution time
    bool fail;

    check_message(skip_calculations, "Atoms haven't moved significantly, " +
            to_string(reader.rms_distance).substr(0,5) + " < " + to_string(conf.distance_tol).substr(0,5)
            + "! Field calculation will be skipped!");

    skip_calculations = true;

    conf.neumann = elfield;
    conf.message = message;
    tstart = omp_get_wtime();

    // Generate FEM mesh
    TetgenMesh bulk_mesh, vacuum_mesh;
    fail = generate_meshes(bulk_mesh, vacuum_mesh);

    bulk_mesh.elems.write  ("output/tetmesh_bulk.vtk");
    vacuum_mesh.elems.write("output/tetmesh_vacuum.vtk");
    bulk_mesh.hexahedra.write  ("output/hexmesh_bulk.vtk");
    vacuum_mesh.hexahedra.write("output/hexmesh_vacuum.vtk");

    if (fail) return 1;
    
    // Solve Laplace equation on vacuum mesh
    fch::Laplace<3> laplace_solver;
    fail = solve_laplace(vacuum_mesh, laplace_solver);

    if (fail) return 1;

    extract_laplace(vacuum_mesh, &laplace_solver);

    // Solve heat & continuum equation on bulk mesh
    static bool odd_run = true;
    if (conf.heating) {
        fail = solve_heat(bulk_mesh, laplace_solver);
        if (!fail) extract_heat(bulk_mesh, ch_solver);

        if (odd_run) { ch_solver = &ch_solver2; prev_ch_solver = &ch_solver1; }
        else { ch_solver = &ch_solver1; prev_ch_solver = &ch_solver2; }
        odd_run = !odd_run;
    }

    start_msg(t0, "=== Saving atom positions...");
    reader.save_current_run_points(conf.distance_tol);
    end_msg(t0);

    cout << "\nTotal time of Femocs.run: " << omp_get_wtime() - tstart << "\n";
    skip_calculations = false;

    return 0;
}

// import atoms from file
const int Femocs::import_atoms(const string& file_name) {
    string file_type, fname;

    if (file_name == "") fname = conf.infile;
    else fname = file_name;

    file_type = get_file_type(fname);
    expect(file_type == "ckx" || file_type == "xyz", "Unknown file type: " + file_type);

    start_msg(t0, "=== Importing atoms...");
    reader.import_file(fname);
    end_msg(t0);
    if (MODES.VERBOSE) cout << "#input atoms: " << reader.get_n_atoms() << endl;

    start_msg(t0, "=== Comparing with previous run...");
    skip_calculations = reader.get_rms_distance(conf.distance_tol) < conf.distance_tol;
    end_msg(t0);

    if (!skip_calculations)
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

    return 0;
}

// import atoms from PARCAS
const int Femocs::import_atoms(int n_atoms, double* coordinates, double* box, int* nborlist) {
    start_msg(t0, "=== Importing atoms...");
    reader.import_parcas(n_atoms, coordinates, box);
    end_msg(t0);
    if (MODES.VERBOSE) cout << "#input atoms: " << reader.get_n_atoms() << endl;

    start_msg(t0, "=== Comparing with previous run...");
    skip_calculations = reader.get_rms_distance(conf.distance_tol) < conf.distance_tol;
    end_msg(t0);

    if (!skip_calculations) {
        start_msg(t0, "=== Calculating coords and atom types...");
        reader.calc_coordination(conf.nnn, conf.coord_cutoff, nborlist);
        reader.extract_types(conf.nnn, conf.latconst);
        end_msg(t0);
    }

    return 0;
}

// import coordinates and types of atoms
const int Femocs::import_atoms(int n_atoms, double* x, double* y, double* z, int* types) {
    start_msg(t0, "=== Importing atoms...");
    reader.import_helmod(n_atoms, x, y, z, types);
    end_msg(t0);
    if (MODES.VERBOSE) cout << "#input atoms: " << reader.get_n_atoms() << endl;

    start_msg(t0, "=== Comparing with previous run...");
    skip_calculations = reader.get_rms_distance(conf.distance_tol) < conf.distance_tol;
    end_msg(t0);

    if (!skip_calculations) {
        start_msg(t0, "=== Calculating coords from atom types...");
        reader.calc_coordination(conf.nnn);
        end_msg(t0);
    }

    return 0;
}

// export the calculated electric field on imported atom coordinates
const int Femocs::export_elfield(int n_atoms, double* Ex, double* Ey, double* Ez, double* Enorm) {
    check_message(vacuum_interpolator.get_n_atoms() == 0, "No solution to export!");

    if (!skip_calculations) {
        start_msg(t0, "=== Interpolating solution...");
        vacuum_interpolation.interpolate(dense_surf, conf.smoothen_solution*conf.coord_cutoff);
        end_msg(t0);

        vacuum_interpolation.write("output/interpolation.movie");
    }

    start_msg(t0, "=== Exporting solution...");
    vacuum_interpolation.export_solution(n_atoms, Ex, Ey, Ez, Enorm);
    end_msg(t0);

    return 0;
}

// linearly interpolate electric field at given points
const int Femocs::interpolate_elfield(int n_points, double* x, double* y, double* z,
        double* Ex, double* Ey, double* Ez, double* Enorm, int* flag) {
    check_message(vacuum_interpolator.get_n_atoms() == 0, "No solution to export!");

    SolutionReader sr(&vacuum_interpolator);
    start_msg(t0, "=== Interpolating electric field...");
    sr.interpolate(n_points, x, y, z, conf.smoothen_solution*conf.coord_cutoff, 1);
    end_msg(t0);
    if (!skip_calculations) sr.write("output/interpolation_E.movie");

    start_msg(t0, "=== Exporting electric field...");
    sr.export_elfield(n_points, Ex, Ey, Ez, Enorm, flag);
    end_msg(t0);

    return 0;
}

// linearly interpolate electric potential at given points
const int Femocs::interpolate_phi(int n_points, double* x, double* y, double* z, double* phi, int* flag) {
    check_message(vacuum_interpolator.get_n_atoms() == 0, "No solution to export!");

    SolutionReader sr(&vacuum_interpolator);
    sr.interpolate(n_points, x, y, z, conf.smoothen_solution*conf.coord_cutoff, 2, false);
    sr.export_potential(n_points, phi, flag);
    if (!skip_calculations) sr.write("output/interpolation_phi.movie");

    return 0;
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
