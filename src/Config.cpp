/*
 * Config.cpp
 *
 *  Created on: 11.11.2016
 *      Author: veske
 */

#include "Config.h"
#include <fstream>
#include <sstream>
#include <algorithm>

using namespace std;
namespace femocs {

// Config constructor
Config::Config() {
    init_values();
}

// Initialize configuration parameters
const void Config::init_values() {
//    infile = "input/rough111.ckx";
//    infile = "input/mushroom2.ckx";
//    infile = "input/tower_hl2p5.ckx";
    infile = "input/nanotip_hr5.ckx";
    latconst = 2.0;

    // conf.infile = home + "input/nanotip_medium.xyz";
    // conf.latconst = 3.61;

    coord_cutoff = 3.1;         // coordination analysis cut-off radius

    nnn = 12;                    // number of nearest neighbours in bulk
    mesh_quality = "2.0";        // minimum mesh quality Tetgen is allowed to make
    nt = 4;                      // number of OpenMP threads
    radius = 10.0;               // inner radius of coarsening cylinder
    coarse_factor = 0.4;         // coarsening factor; bigger number gives coarser surface
    smooth_factor = 0.5;         // surface smoothing factor; bigger number gives smoother surface
    n_bins = 20;                 // number of bins in histogram smoother
    postprocess_marking = false; // make extra effort to mark correctly the vacuum nodes in shadow area
    rmin_rectancularize = latconst / 1.0; // 1.5+ for <110> simubox, 1.0 for all others

    refine_apex = false;         // refine nanotip apex
    distance_tol = 0.5*latconst;

    // Electric field is applied 100 lattice constants above the highest point of surface
    // and bulk is extended 20 lattice constants below the minimum point of surface
    zbox_above = 100 * latconst;
    zbox_below = 20 * latconst;
}

// Remove the noise from the beginning and end of the string
const void Config::trim(string& str) {
    str.erase(0, str.find_first_of(comment_symbols + data_symbols));
    str.erase(str.find_last_of(data_symbols) + 1);
}

// Read the configuration parameters from input script
const void Config::read_all(const string& file_name) {
    ifstream file(file_name);
    require(file, "File not found: " + file_name);

    string line;
    data.clear();

    // loop through the lines in a file
    while (getline(file, line)) {
        // force all the characters in a line to lower case
        std::transform(line.begin(), line.end(), line.begin(), ::tolower);

        bool line_started = true;
        // store the command and its parameters from non-empty and non-pure-comment lines
        while(line.size() > 0) {
            trim(line);
            int i = line.find_first_not_of(data_symbols);
            if (i <= 0) break;

            if (line_started) data.push_back({});
            data.back().push_back( line.substr(0, i) );
            line = line.substr(i);
            line_started = false;
        }
    }

    // Modify the parameters that are correctly specified in input script
    read_parameter("infile", infile);
    read_parameter("latconst", latconst);
    read_parameter("coord_cutoff", coord_cutoff);
    read_parameter("nnn", nnn);
    read_parameter("mesh_quality", mesh_quality);
    read_parameter("radius", radius);
    read_parameter("coarse_factor", coarse_factor);
    read_parameter("smooth_factor", smooth_factor);
    read_parameter("n_bins", n_bins);
    read_parameter("postprocess_marking", postprocess_marking);
    read_parameter("refine_apex", refine_apex);
    read_parameter("distance_tol", distance_tol);
    read_parameter("rmin_rectancularize", rmin_rectancularize);
    read_parameter("zbox_above", zbox_above);
    read_parameter("zbox_below", zbox_below);
}

// Look up the parameter with string argument
const void Config::read_parameter(const string& param, string& arg) {
    // loop through all the commands that were found from input script
    for (vector<string> str : data) {
        if (str.size() >= 2 && str[0] == param) {
            arg = str[1];
            return;
        }
    }
}

// Look up the parameter with boolean argument
const void Config::read_parameter(const string& param, bool& arg) {
    // loop through all the commands that were found from input script
    for (vector<string> str : data)
        if (str.size() >= 2 && str[0] == param) {
            istringstream is1(str[1]);
            istringstream is2(str[1]);
            bool result;
            // try to parse the bool argument in text format
            if (is1 >> std::boolalpha >> result) arg = result;
            // try to parse the bool argument in numeric format
            else if (is2 >> result) arg = result;
            return;
        }
}

// Look up the parameter with integer argument
const void Config::read_parameter(const string& param, int& arg) {
    // loop through all the commands that were found from input script
    for (vector<string> str : data)
        if (str.size() >= 2 && str[0] == param) {
            istringstream is(str[1]); int result;
            if (is >> result) arg = result;
            return;
        }
}

// Look up the parameter with double argument
const void Config::read_parameter(const string& param, double& arg) {
    // loop through all the commands that were found from input script
    for (vector<string> str : data) {
        if (str.size() >= 2 && str[0] == param) {
            istringstream is(str[1]); double result;
            if (is >> result) arg = result;
            return;
        }
    }
}

} // namespace femocs
