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

cube_t initialize_cube(real origin[DIM], real boxv[DIM][DIM], int n[DIM], atom_t *, int);
int * cubes_larger_than(real cutoff, cube_t * cube);
int * invert_indices(int nvoxels, int * indices);
void get_cell_pointer(cube_t * cube, real * cell);
real * get_box_volels(cube_t * cube);
void get_box_volels_pointer(cube_t * cube, real * dx);
void get_box_areas_pointer (real * da, cube_t * cube, real * dx );
cube_t interpolate_cube_trilinear ( cube_t * original, int factor, int periodic );
#ifdef HAVE_EINSPLINE
cube_t interpolate_cube_bsplines ( cube_t * original, int factor, int periodic );
#include <einspline/bspline_base.h>
#include <einspline/bspline_structs.h>
UBspline_3d_s * get_cube_bsplines ( cube_t * cube, int periodic );
BCtype_s set_boundary_conditions_bsplines ( bc_code lp, bc_code rp, float lVal, float rVal );
#endif
cube_t local_interpolation ( cube_t *cube, real *point, int lint, int interpolkind, int ninterpol, char *outputprefix, real *pbc, int periodic );
