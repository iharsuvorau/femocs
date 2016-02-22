/*
 * Mesh.cpp
 *
 *  Created on: 1.2.2016
 *      Author: veske
 */

#include "Mesh.h"

#include <stdio.h>
#include <cstring>
#include <iostream>
#include <algorithm>

using namespace std;
namespace femocs {

Mesh::Mesh() {
    inodes = 0;
    ielems = 0;
    ifaces = 0;
    inodemarker = 0;

    stat.Vmin = stat.Vmax = stat.Vmedian = stat.Vaverage = 0;
}

// =================================
// *** GETTERS: ***************

const double Mesh::getNode(int i, int xyz) {
#if DEBUGMODE
    if(i < 0 || i > getNnodes() || xyz < 0 || xyz > 2) {
        cout << "Index out of number of nodes bounds" << endl;
        return -1.0;
    }
#endif
    return tetIO.pointlist[3*i+xyz];
}

const int Mesh::getFace(int i, int node) {
#if DEBUGMODE
    if(  (i < 0) || (i > getNfaces()) || (node < 0) || (node > 2) ) {
        cout << "Index out of number of faces bounds" << endl;
        return -1;
    }
#endif
    return tetIO.trifacelist[3*i+node];
}

const int Mesh::getElem(int i, int node) {

#if DEBUGMODE
    if( (i < 0) || (i > getNelems()) || (node < 0) || (node > 3) ) {
        cout << "Index out of number of elems bounds" << endl;
        return -1;
    }
#endif
    return tetIO.tetrahedronlist[4*i+node];
}

double* Mesh::getNodes() {
    return tetIO.pointlist;
}

int* Mesh::getFaces() {
    return tetIO.trifacelist;
}

int* Mesh::getElems() {
    return tetIO.tetrahedronlist;
}

double Mesh::getVolume(const int i) {
    return volumes[i];
}

const int Mesh::getNodemarker(const int i) {
#if DEBUGMODE
    if (i >= tetIO.numberofpoints) {
        cout << "Index exceeds node attributes size!" << endl;
        return -1;
    }
#endif
    return tetIO.pointmarkerlist[i];
    //    return nodemarkers[i];
}

const int Mesh::getFacemarker(const int i) {
    return facemarkers[i];
}

const int Mesh::getElemmarker(const int i) {
    return elemmarkers[i];
}

int* Mesh::getNodemarkers() {
    return tetIO.pointmarkerlist;
}

vector<int>* Mesh::getFacemarkers() {
    return &facemarkers;
}

vector<int>* Mesh::getElemmarkers() {
    return &elemmarkers;
}

const int Mesh::getNnodes() {
    return tetIO.numberofpoints;
}

const int Mesh::getNelems() {
    return tetIO.numberoftetrahedra;
}

const int Mesh::getNfaces() {
    return tetIO.numberoftrifaces;
}

const int Mesh::getNnodemarkers() {
    return tetIO.numberofpoints;
    //    return nodemarkers.size();
}

const int Mesh::getNfacemarkers() {
    return facemarkers.size();
}

const int Mesh::getNelemmarkers() {
    return elemmarkers.size();
}

const int Mesh::getNvolumes() {
    return volumes.size();
}

// =================================
// *** INITIALIZERS: ***************

void Mesh::init_nodemarkers(const int N) {
//    nodemarkers.reserve(N);
//    tetIO.numberofpointattributes = N;
    tetIO.pointmarkerlist = new int[N];
    inodemarker = 0;
}

void Mesh::init_facemarkers(const int N) {
    facemarkers.reserve(N);
}

void Mesh::init_elemmarkers(const int N) {
    elemmarkers.reserve(N);
}

void Mesh::init_nodes(const int N) {
    inodes = 0;
    tetIO.numberofpoints = N;
    tetIO.pointlist = new REAL[3 * N];
}

void Mesh::init_faces(const int N) {
    ifaces = 0;
    tetIO.numberoftrifaces = N;
    tetIO.trifacelist = new int[3 * N];
}

void Mesh::init_elems(const int N) {
    ielems = 0;
    tetIO.numberoftetrahedra = N;
    tetIO.tetrahedronlist = new int[4 * N];
}

void Mesh::init_volumes(const int N) {
    volumes.reserve(N);
}

// =================================
// *** ADDERS: ***************

void Mesh::add_volume(const double V) {
    volumes.push_back(V);
}

void Mesh::add_nodemarker(const int m) {
#if DEBUGMODE
    if (inodemarker >= tetIO.numberofpoints) {
        cout << "Node marker list is full!" << endl;
        return;
    }
#endif
    tetIO.pointmarkerlist[inodemarker] = m;
    inodemarker++;
//    nodemarkers.push_back(m);
}

void Mesh::add_facemarker(const int m) {
    facemarkers.push_back(m);
}

void Mesh::add_elemmarker(const int m) {
    elemmarkers.push_back(m);
}

void Mesh::add_elem(const int e1, const int e2, const int e3, const int e4) {
    int i = 4 * ielems;
    tetIO.tetrahedronlist[i + 0] = e1;
    tetIO.tetrahedronlist[i + 1] = e2;
    tetIO.tetrahedronlist[i + 2] = e3;
    tetIO.tetrahedronlist[i + 3] = e4;
    ielems++;
}

void Mesh::add_face(const int f1, const int f2, const int f3) {
    int i = 3 * ifaces;
    tetIO.trifacelist[i + 0] = f1;
    tetIO.trifacelist[i + 1] = f2;
    tetIO.trifacelist[i + 2] = f3;
    ifaces++;
}

void Mesh::add_node(const double x, const double y, const double z) {
    int i = 3 * inodes;
    tetIO.pointlist[i + 0] = (REAL) x;
    tetIO.pointlist[i + 1] = (REAL) y;
    tetIO.pointlist[i + 2] = (REAL) z;
    inodes++;
}

// =================================
// *** REPLICATORS: ***************

void Mesh::copy_statistics(shared_ptr<Mesh> mesh) {
    stat.Vmin = mesh->stat.Vmin;
    stat.Vmax = mesh->stat.Vmax;
    stat.Vaverage = mesh->stat.Vaverage;
    stat.Vmedian = mesh->stat.Vmedian;
}

void Mesh::copy_nodes(shared_ptr<Mesh> mesh) {
    int N = mesh->getNnodes();
    for (int i = 0; i < 3 * N; ++i)
        tetIO.pointlist[i] = mesh->getNodes()[i];
    inodes = N;
}

void Mesh::copy_faces(shared_ptr<Mesh> mesh, const int offset) {
    int N = mesh->getNfaces();
    if (offset == 0)
        for (int i = 0; i < 3 * N; ++i)
            tetIO.trifacelist[i] = mesh->getFaces()[i];
    else
        for (int i = 0; i < 3 * N; ++i)
            tetIO.trifacelist[i] = offset + mesh->getFaces()[i];
    ifaces = N;
}

void Mesh::copy_elems(shared_ptr<Mesh> mesh, const int offset) {
    int N = mesh->getNelems();
    if (offset == 0)
        for (int i = 0; i < 4 * N; ++i)
            tetIO.tetrahedronlist[i] = mesh->getElems()[i];
    else
        for (int i = 0; i < 4 * N; ++i)
            tetIO.tetrahedronlist[i] = offset + mesh->getElems()[i];
    ielems = N;
}

void Mesh::copy_nodemarkers(shared_ptr<Mesh> mesh) {
    int N = mesh->getNnodemarkers();
    for (int i = 0; i < N; ++i)
        tetIO.pointmarkerlist[i] = mesh->getNodemarker(i);
    inodemarker = N;
}

void Mesh::copy_facemarkers(shared_ptr<Mesh> mesh) {
    int N = mesh->getNfacemarkers();
    for (int i = 0; i < N; ++i)
        facemarkers.push_back(mesh->getFacemarker(i));
}

void Mesh::copy_elemmarkers(shared_ptr<Mesh> mesh) {
    int N = mesh->getNelemmarkers();
    for (int i = 0; i < N; ++i)
        elemmarkers.push_back(mesh->getElemmarker(i));
}

// =================================
// *** VARIA: ***************

void Mesh::calc_volumes() {
    int i, j, k, n1, n2, n3, n4;
    double V;
    double u[3], v[3], w[3];

    const int ncoords = 3; // nr of coordinates
    const int nnodes = 4;  // nr of nodes per element
    int N = getNelems();
    double* nodes = getNodes();
    int* elems = getElems();

    // Loop through the elements
    for (i = 0; i < N; ++i) {
        j = nnodes * i;
        // Loop through x, y and z coordinates
        for (k = 0; k < ncoords; ++k) {
            n1 = ncoords * elems[j + 0] + k; // index of x, y or z coordinate of 1st node
            n2 = ncoords * elems[j + 1] + k; // ..2nd node
            n3 = ncoords * elems[j + 2] + k; // ..3rd node
            n4 = ncoords * elems[j + 3] + k; // ..4th node

            u[k] = nodes[n1] - nodes[n2];
            v[k] = nodes[n1] - nodes[n3];
            w[k] = nodes[n1] - nodes[n4];
        }
        V = u[0] * (v[1] * w[2] - v[2] * w[1]) - u[1] * (v[0] * w[2] - v[2] * w[0])
                + u[2] * (v[0] * w[1] - v[1] * w[0]);
        add_volume(fabs(V) / 6.0);
    }
}

void Mesh::calc_volume_statistics() {
    size_t size = volumes.size();
    // Make a copy of the volumes vector
    vector<double> tempvec;
    tempvec.reserve(size);
    copy(volumes.begin(), volumes.end(), back_inserter(tempvec));

    sort(tempvec.begin(), tempvec.end());

    stat.Vmin = tempvec[0];
    stat.Vmax = tempvec[size - 1];
    stat.Vaverage = accumulate(tempvec.begin(), tempvec.end(), 0) / size;

    if (size % 2 == 0)
        stat.Vmedian = (tempvec[size / 2 - 1] + tempvec[size / 2]) / 2;
    else
        stat.Vmedian = tempvec[size / 2];
}

void Mesh::transform_elemmarkers() {
    int N = tetIO.numberoftetrahedronattributes;

    cout << "nelemmarkers=" << N << endl;

    init_elemmarkers(N);
    for (int i = 0; i < N; ++i)
        add_elemmarker((int) 10.0 * tetIO.tetrahedronattributelist[i]);
}

// Function to perform tetgen calculation on input and output data
void Mesh::recalc(const string cmd) {
    tetgenbeh.parse_commandline(const_cast<char*>(cmd.c_str()));
    tetrahedralize(&tetgenbeh, &tetIO, &tetIO);
}

void Mesh::output(const string cmd) {
    tetgenbeh.parse_commandline(const_cast<char*>(cmd.c_str()));
    tetrahedralize(&tetgenbeh, &tetIO, NULL);
}

// Function to output mesh in .vtk format
void Mesh::write_vtk(const string file_name, const int nnodes, const int ncells,
        const int nnodemarkers, const int nmarkers, const REAL* nodes, const int* cells,
        const int* nodemarkers, const vector<int>* markers, const int celltype,
        const int nnodes_in_cell) {
    int i, j;
    char file_name_char[1024];
    strcpy(file_name_char, file_name.c_str());

    FILE *outfile;
    outfile = fopen(file_name_char, "w");
    if (outfile == (FILE *) NULL) {
        printf("File I/O Error:  Cannot create file %s.\n", file_name_char);
        return;
    }

//    fprintf(outfile, "# vtk DataFile Version 2.0\n");
    fprintf(outfile, "# vtk DataFile Version 3.0\n");
    fprintf(outfile, "# Unstructured grid\n");
    fprintf(outfile, "ASCII\n"); // another option is BINARY
    fprintf(outfile, "DATASET UNSTRUCTURED_GRID\n\n");

    // Output the nodes
    if (nnodes > 0) {
        fprintf(outfile, "POINTS %d double\n", nnodes);
        for (i = 0; i < 3 * nnodes; i += 3)
            fprintf(outfile, "%.8g %.8g %.8g\n", nodes[i + 0], nodes[i + 1], nodes[i + 2]);
        fprintf(outfile, "\n");
    }

    // Output the cells (tetrahedra or triangles)
    if (ncells > 0) {
        fprintf(outfile, "CELLS %d %d\n", ncells, ncells * (nnodes_in_cell + 1));
        for (i = 0; i < nnodes_in_cell * ncells; i += nnodes_in_cell) {
            fprintf(outfile, "%d ", nnodes_in_cell);
            for (j = 0; j < nnodes_in_cell; ++j)
                fprintf(outfile, "%d ", cells[i + j]);
            fprintf(outfile, "\n");
        }
        fprintf(outfile, "\n");
    }

    // Output the types of cells, 10=tetrahedron, 5=triangle
    if (ncells > 0) {
        fprintf(outfile, "CELL_TYPES %d\n", ncells);
        for (i = 0; i < ncells; ++i)
            fprintf(outfile, "%d ", celltype);
//            fprintf(outfile, "%d\n", celltype);
        fprintf(outfile, "\n\n");
    }

//    // Output point attributes
//    if (nnodemarkers > 0) {
//        fprintf(outfile, "POINT_DATA %d\n", nnodemarkers);
//        fprintf(outfile, "SCALARS Point_markers int\n");
//        fprintf(outfile, "LOOKUP_TABLE default\n");
//        for (i = 0; i < nnodemarkers; ++i)
//            fprintf(outfile, "%d\n", nodemarkers[i]);
//        fprintf(outfile, "\n");
//    }

    // Output cell attributes
    if (nmarkers > 0) {
        fprintf(outfile, "CELL_DATA %d\n", nmarkers);
        fprintf(outfile, "SCALARS Cell_markers int\n");
        fprintf(outfile, "LOOKUP_TABLE default\n");
        for (i = 0; i < nmarkers; ++i)
            fprintf(outfile, "%d\n", (*markers)[i]);
        fprintf(outfile, "\n");
    }

    fclose(outfile);
}

// Function to output faces in .vtk format
void Mesh::write_faces(const string file_name) {
    const int celltype = 5; // 5-triangle, 10-tetrahedron
    const int nnodes_in_cell = 3;

    int nnodes = getNnodes();
    int nfaces = getNfaces();
    int nmarkers = getNfacemarkers();
    int nnodemarkers = getNnodemarkers();
    REAL* nodes = getNodes();          // pointer to nodes
    int* faces = getFaces();         // pointer to face nodes
    int* nodemakers = getNodemarkers();
    vector<int>* cellmarkers = &facemarkers;    // pointer to face markers

//    write_vtk(file_name, nnodes, nfaces, nmarkers, nodes, faces, markers, celltype, nnodes_in_cell);
    write_vtk(file_name, nnodes, nfaces, nnodemarkers, nmarkers, nodes, faces, nodemakers,
            cellmarkers, celltype, nnodes_in_cell);
}

// Function to output faces in .vtk format
void Mesh::write_elems(const string file_name) {
    const int celltype = 10; // 5-triangle, 10-tetrahedron
    const int nnodes_in_cell = 4;

    int nnodes = getNnodes();
    int nelems = getNelems();
    int nmarkers = getNelemmarkers();
    int nnodemarkers = getNnodemarkers();
    REAL* nodes = getNodes();          // pointer to nodes
    int* elems = getElems();     // pointer to faces nodes
    int* nodemakers = getNodemarkers();
    vector<int>* cellmarkers = &elemmarkers;    // pointer to element markers

    //write_vtk(file_name, nnodes, nelems, nmarkers, nodes, elems, markers, celltype, nnodes_in_cell);

    write_vtk(file_name, nnodes, nelems, nnodemarkers, nmarkers, nodes, elems, nodemakers,
            cellmarkers, celltype, nnodes_in_cell);
}

// =================================
} /* namespace femocs */