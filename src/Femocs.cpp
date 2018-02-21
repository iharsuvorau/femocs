/*
 * Femocs.cpp
 *
 *  Created on: 14.12.2015
 *      Author: veske
 */

#include "Femocs.h"
#include "Globals.h"
#include "ProjectNanotip.h"
#include "ProjectRunaway.h"

using namespace std;
namespace femocs {

// specify simulation parameters
Femocs::Femocs(const string &conf_file) : t0(0), skip_meshing(false) {
    static bool first_call = true;
    bool fail;

    // Read configuration parameters from configuration file
    conf.read_all(conf_file);

    // Initialise file writing
    MODES.WRITEFILE = conf.behaviour.n_writefile > 0;

    // Pick the correct verbosity mode flags
    if      (conf.behaviour.verbosity == "mute")    { MODES.MUTE = true;  MODES.VERBOSE = false; }
    else if (conf.behaviour.verbosity == "silent")  { MODES.MUTE = false; MODES.VERBOSE = false; }
    else if (conf.behaviour.verbosity == "verbose") { MODES.MUTE = false; MODES.VERBOSE = true;  }

    // Clear the results from previous run
    if (first_call && conf.run.output_cleaner) fail = system("rm -rf out");
    fail = system("mkdir -p out");
    first_call = false;

    write_verbose_msg("======= Femocs started! =======");
//    project = new ProjectNanotip(reader, conf);
    project = new ProjectRunaway(reader, conf);
}

// delete data and print bye-bye-message
Femocs::~Femocs() {
    delete project;
    write_verbose_msg("======= Femocs finished! =======");
}

// Write all the available data to file for debugging purposes
int Femocs::force_output() {
    return project->force_output();
}

// Generate FEM mesh and solve differential equation(s)
int Femocs::run(const int timestep) {
    return project->run(timestep);
}

// Generate FEM mesh and solve differential equation(s)
int Femocs::run(const double elfield, const string &timestep) {
    // convert message to integer time step
    stringstream parser;
    int tstep;
    parser << timestep;
    parser >> tstep;
    parser.flush();

    return project->run(tstep);
}

// Generate artificial nanotip
int Femocs::generate_nanotip(const double height, const double radius, const double resolution) {
    clear_log();

    double res = conf.geometry.latconst;
    if (resolution > 0) res = resolution;

    double r = conf.geometry.radius - res;
    if (radius > 0) r = radius;

    start_msg(t0, "=== Generating nanotip...");
    reader.generate_nanotip(height, r, res);
    reader.calc_coordinations(conf.geometry.nnn);
    end_msg(t0);
    write_verbose_msg( "#input atoms: " + to_string(reader.size()) );

    reader.write("out/atomreader.ckx");
    return 0;
}

// import atoms from a file
int Femocs::import_atoms(const string& file_name, const int add_noise) {
    clear_log();
    string file_type, fname;

    if (file_name == "") fname = conf.path.infile;
    else fname = file_name;

    file_type = get_file_type(fname);
    require(file_type == "ckx" || file_type == "xyz", "Unknown file type: " + file_type);

    start_msg(t0, "=== Importing atoms...");
    reader.import_file(fname, add_noise);
    end_msg(t0);
    write_verbose_msg( "#input atoms: " + to_string(reader.size()) );

    start_msg(t0, "=== Comparing with previous run...");
    skip_meshing = reader.calc_rms_distance(conf.tolerance.distance) < conf.tolerance.distance;
    end_msg(t0);

    if (!skip_meshing) {
        if (file_type == "xyz") {
            start_msg(t0, "=== Performing coordination analysis...");
            if (!conf.run.rdf) reader.calc_coordinations(conf.geometry.nnn, conf.geometry.coordination_cutoff);
            else reader.calc_coordinations(conf.geometry.nnn, conf.geometry.latconst, conf.geometry.coordination_cutoff);
            end_msg(t0);

            if (conf.run.rdf) {
                stringstream stream;
                stream << fixed << setprecision(3);
                stream << "nnn: " << conf.geometry.nnn << ", latconst: " << conf.geometry.latconst
                        << ", coord_cutoff: " << conf.geometry.coordination_cutoff
                        << ", cluster_cutoff: " << conf.geometry.cluster_cutoff;
                write_verbose_msg(stream.str());
            }

            if (conf.run.cluster_anal) {
                start_msg(t0, "=== Performing cluster analysis...");
                reader.calc_clusters(conf.geometry.cluster_cutoff, conf.geometry.coordination_cutoff);
                end_msg(t0);
                reader.check_clusters(1);
            }

            start_msg(t0, "=== Extracting atom types...");
            reader.extract_types(conf.geometry.nnn, conf.geometry.coordination_cutoff);
            end_msg(t0);

        } else {
            start_msg(t0, "=== Calculating coords from atom types...");
            reader.calc_coordinations(conf.geometry.nnn);
            end_msg(t0);
        }
    }

    reader.write("out/atomreader.ckx");
    return 0;
}

// import atoms from PARCAS
int Femocs::import_atoms(const int n_atoms, const double* coordinates, const double* box, const int* nborlist) {
    clear_log();

    start_msg(t0, "=== Importing atoms...");
    reader.import_parcas(n_atoms, coordinates, box);
    end_msg(t0);
    write_verbose_msg( "#input atoms: " + to_string(reader.size()) );

    start_msg(t0, "=== Comparing with previous run...");
    skip_meshing = reader.calc_rms_distance(conf.tolerance.distance) < conf.tolerance.distance;
    end_msg(t0);

    if (!skip_meshing) {
        start_msg(t0, "=== Performing coordination analysis...");
        if (!conf.run.rdf) reader.calc_coordinations(conf.geometry.nnn, conf.geometry.coordination_cutoff, nborlist);
        else reader.calc_coordinations(conf.geometry.nnn, conf.geometry.coordination_cutoff, conf.geometry.latconst, nborlist);
        end_msg(t0);

        if (conf.run.rdf) {
            stringstream stream;
            stream << fixed << setprecision(3)
                            << "nnn: " << conf.geometry.nnn << ", latconst: " << conf.geometry.latconst
                            << ", coord_cutoff: " << conf.geometry.coordination_cutoff
                            << ", cluster_cutoff: " << conf.geometry.cluster_cutoff;
            write_verbose_msg(stream.str());
        }

        if (conf.run.cluster_anal) {
            start_msg(t0, "=== Performing cluster analysis...");
            reader.calc_clusters(conf.geometry.nnn, conf.geometry.cluster_cutoff, conf.geometry.coordination_cutoff, nborlist);
            end_msg(t0);
            reader.check_clusters(1);
        }

        start_msg(t0, "=== Extracting atom types...");
        reader.extract_types(conf.geometry.nnn, conf.geometry.latconst);
        end_msg(t0);
    }

    reader.write("out/atomreader.ckx");
    return 0;
}

// import coordinates and types of atoms
int Femocs::import_atoms(const int n_atoms, const double* x, const double* y, const double* z, const int* types) {
    clear_log();
    conf.run.surface_cleaner = false; // disable the surface cleaner for atoms with known types

    start_msg(t0, "=== Importing atoms...");
    reader.import_helmod(n_atoms, x, y, z, types);
    end_msg(t0);
    write_verbose_msg( "#input atoms: " + to_string(reader.size()) );

    start_msg(t0, "=== Comparing with previous run...");
    skip_meshing = reader.calc_rms_distance(conf.tolerance.distance) < conf.tolerance.distance;
    end_msg(t0);

    if (!skip_meshing) {
        start_msg(t0, "=== Calculating coordinations from atom types...");
        reader.calc_coordinations(conf.geometry.nnn);
        end_msg(t0);
    }

    reader.write("out/atomreader.ckx");
    return 0;
}

// export the atom types as seen by FEMOCS
int Femocs::export_atom_types(const int n_atoms, int* types) {
    const int export_size = min(n_atoms, reader.size());
    for (int i = 0; i < export_size; ++i)
        types[i] = reader.get_marker(i);

    return reader.check_clusters(0);
}

// export electric field on imported atom coordinates
int Femocs::export_elfield(const int n_atoms, double* Ex, double* Ey, double* Ez, double* Enorm) {
    return project->fields.export_elfield(n_atoms, Ex, Ey, Ez, Enorm);
}

// calculate and export temperatures on imported atom coordinates
int Femocs::export_temperature(const int n_atoms, double* T) {
    return project->temperatures.export_temperature(n_atoms, T);
}

// export charges & forces on imported atom coordinates
int Femocs::export_charge_and_force(const int n_atoms, double* xq) {
    return project->forces.export_charge_and_force(n_atoms, xq);
}

// export forces & pair potentials on imported atom coordinates
int Femocs::export_force_and_pairpot(const int n_atoms, double* xnp, double* Epair, double* Vpair) {
    return project->forces.export_force_and_pairpot(n_atoms, xnp, Epair, Vpair);;
}

// linearly interpolate electric field at given points
int Femocs::interpolate_surface_elfield(const int n_points, const double* x, const double* y, const double* z,
        double* Ex, double* Ey, double* Ez, double* Enorm, int* flag) {
    return project->fields.interpolate_surface_elfield(n_points, x, y, z, Ex, Ey, Ez, Enorm, flag);
}

// linearly interpolate electric field at given points
int Femocs::interpolate_elfield(const int n_points, const double* x, const double* y, const double* z,
        double* Ex, double* Ey, double* Ez, double* Enorm, int* flag) {
    return project->fields.interpolate_elfield(n_points, x, y, z, Ex, Ey, Ez, Enorm, flag);
}

// linearly interpolate electric potential at given points
int Femocs::interpolate_phi(const int n_points, const double* x, const double* y, const double* z,
        double* phi, int* flag) {
    return project->fields.interpolate_phi(n_points, x, y, z, phi, flag);
}

int Femocs::export_results(const int n_points, const char cmd, double* data) {
    return project->export_results(n_points, LABELS.decode(cmd), data);
}

int Femocs::interpolate_results(const int n_points, const char cmd,
        const double* x, const double* y, const double* z, double* data, int* flag) {
    return project->interpolate_results(n_points, LABELS.decode(cmd), false, x, y, z, data, flag);
}

int Femocs::interpolate_surface_results(const int n_points, const char cmd,
        const double* x, const double* y, const double* z, double* data, int* flag) {
    return project->interpolate_results(n_points, LABELS.decode(cmd), true, x, y, z, data, flag);
}

// parse integer argument of the command from input script
int Femocs::parse_command(const string& command, int* arg) {
    return conf.read_command(command, arg[0]);
}

// parse double argument of the command from input script
int Femocs::parse_command(const string& command, double* arg) {
    return conf.read_command(command, arg[0]);
}

// parse string argument of the command from input script
int Femocs::parse_command(const string& command, string& arg) {
    return conf.read_command(command, arg);
}

// parse char array argument of the command from input script
int Femocs::parse_command(const string& command, char* arg) {
    string string_arg;
    bool fail = conf.read_command(command, string_arg);
    if (!fail) string_arg.copy(arg, string_arg.length());
    return fail;
}

} // namespace femocs
