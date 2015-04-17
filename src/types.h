/*

SURF is a C program to generate and analyse instantaneous liquid interfaces.
Copyright 2015 Frank Uhlig (uhlig.frank@gmail.com)

This file is part of SURF.

SURF is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

SURF is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with SURF.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "declarations.h"

#ifdef SINGLE
typedef float real;
#else
typedef double real;
#endif

static const char *keywords[NUMKEYS] =
{ 
    "task",
    "output",
    "mask",
    "structure",
    "surfacecutoff",
    "zeta",
    "roughsurf",
    "batch",
    "oprefix",
    "preinterpolate",
    "postinterpolate",
    "surfrefinement",
    "refinementitpl",
    "pbc",
    "solcenter",
    "fragments",
    "periodic",
    "blfudge",
    "guessfragments",
    "trajectory",
    "refmask",
    "resolution",
    "xdrread",
    "ignorefirst",
    "accuracy",
    "dummy",
    "direction",
};

typedef struct voxel_s
{
    real data;
    real coords[DIM];
    int ix[DIM];
} voxel_t;

typedef struct atom_s
{
    real coords[DIM];
    int number;
    real charge;
    real cfcharge;
    real rvdw;
    char symbol[4];
    real valencecoreq;
    int coreel;
    real covrad;
    real mass;
} atom_t;

typedef struct vector_s
{
    real entry[DIM];
} vector_t;

typedef struct cube_s
{
    char title[MAXSTRLEN];
    char comment[MAXSTRLEN];
    int natoms;
    real origin[DIM];
    int n[DIM];
    real boxv[DIM][DIM];
    int nvoxels;
    atom_t * atoms;
    voxel_t * voxels;
    int orthogonal;
    int cubic;
    real dv;
} cube_t;

typedef struct input_s
{
    char task[MAXSTRLEN];
    int tasknum;
    char inpfile[MAXSTRLEN];
    int output;
    char mask[MAXSTRLEN];
    char maskkind[MAXSTRLEN];
    char refmask[MAXSTRLEN];
    char refmaskkind[MAXSTRLEN];
    int nkinds;
    int refnkinds;
    real surfacecutoff;
    real zeta;
    int roughsurf;
    char structure[MAXSTRLEN];
    char outputprefix[MAXSTRLEN];
    char fragmask[MAXSTRLEN];
    int fragtotnatoms;
    int start;
    int stop;
    int stride;
    int batchmode;
    int postinterpolate;
    int surfrefinement;
    int refineitpl;
    real pbc[DIM];
    real solcenter[DIM];
    int solset;
    int pbcset;
    real refcenter[DIM];
    int numfrags;
    int * natomsfrag;
    char * fragments[MAXSTRLEN];
    int periodic;
    int wrap;
    real blfudge;
    int refcenterset;
    int guessfragments;
    int nofrags;
    char trajectory[MAXSTRLEN];
    int trajmode;
    real resolution;
    int xdrread;
    int gnrfrst;
    int othercenter;
    real accuracy;
    int rprof_num;
    atom_t dumatom;
    int dummy;
    int direction;
} input_t;

typedef struct list_s
{
    int element;
    struct list_s * next;
} list_t;

typedef struct badervol_s
{
    list_t * head;
    atom_t * atoms;
} badervol_t;


typedef enum runtype_e
{
    NOTASK=1000,
    BADER,
    MOMENTS,
    FRAGMENTS,
    QMMM_ASSIGN,
    QMMM_ASSIGN_TUNNEL,
    RDF,
    SURFDIST,
    DENSPROF,
    TOPOLOGY,

} runtype_t;

typedef enum abstruse_e
{
    USERDEFINED=-1,
    BOXCENTER=-2,
    BOXEDGELO=-3,
    BOXEDGEHI=-4,
} abstruse_t;