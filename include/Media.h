/*
 * Media.h
 *
 *  Created on: 5.4.2016
 *      Author: veske
 */

#ifndef MEDIA_H_
#define MEDIA_H_

#include "Config.h"
#include "Macros.h"
#include "Coarseners.h"
#include "Medium.h"
#include "AtomReader.h"
#include "LinearInterpolator.h"
#include "VoronoiMesh.h"

using namespace std;
namespace femocs {

/** Routines and data related to making a Surface */
class Media: public Medium {
public:
    /** Empty Media constructor */
    Media();

    /** Initialise empty Media and reserve memory for atoms */
    Media(const int n_atoms);

    /** Generate Media with 4 atoms on simubox edges */
    Media(const Medium::Sizes& sizes, const double z);

    /** Generate system with 4 atoms at the simubox corners */
    void generate_simple(const Medium::Sizes& sizes, const double z);

    /** Extract atom with desired types and clean it from lonely atoms;
     * atom is considered lonely if its coordination is lower than threshold.
     * @param reader  AtomReader holding the atoms and their types
     * @param type    type of the atoms that will be read
     * @param invert  if true all the atoms except the 'type'-ones will be stored
     */
    void extract(const AtomReader& reader, const int type, const bool invert=false);

    /** Extend the flat area by generating additional atoms */
    Media extend(const double latconst, const double box_width, Coarseners &coarseners);

    /** Extend the flat area by reading additional atoms */
    Media extend(const string &file_name, Coarseners &coarseners);

    /** Coarsen the atoms by generating additional boundary nodes and then running cleaner */
    Media coarsen(Coarseners &coarseners);

    /** Clean the surface from atoms that are too close to each other */
    Media clean(Coarseners &coarseners);

    /** Increase or decrease the total volume of system without altering the centre of mass */
    void transform(const double latconst);

    /** Remove the atoms that are too far from surface faces */
    void clean_by_triangles(vector<int>& surf2face, const TriangleInterpolator& interpolator, const double r_cut);

    int clean_by_voronois(const double radius, const double latconst, const string& mesh_quality);

    /** Smoothen the atoms inside the cylinder */
    void smoothen(const double radius, const double smooth_factor, const double r_cut);

    /** Smoothen all the atoms in the system */
    void smoothen(const double smooth_factor, const double r_cut);

    /** Smoothen the atoms using Taubin lambda|mu smoothing algorithm */
    void smoothen(const Config& conf, const double r_cut);

private:
    /** Function used to smoothen the atoms */
    inline double smooth_function(const double distance, const double smooth_factor) const;

    /** Calculate neighbour list for atoms.
     * Atoms are considered neighbours if the distance between them is no more than r_cut. */
    void calc_nborlist(vector<vector<unsigned>>& nborlist, const int nnn, const double r_cut) ;

    /** Smoothen the atoms using Taubin lambda|mu algorithm with inverse neighbour count weighting */
    void laplace_smooth(const double scale, const vector<vector<unsigned>>& nborlist);

    /** Generate edge with regular atom distribution between surface corners */
    void generate_middle(const Medium::Sizes& ar_sizes, const double z, const double dist);

    /** Separate cylindrical region from substrate region */
    void get_nanotip(Media& nanotip, const double radius);

    int get_nanotip(Media& nanotip, vector<bool>& atom_in_nanotip, const double radius);

    int calc_voronois(VoronoiMesh& voromesh, vector<bool>& node_in_nanotip,
            const double radius, const double latconst, const string& mesh_quality);
};

} /* namespace femocs */

#endif /* MEDIA_H_ */
