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
#include "types.h"
#include "constants.h"
#include "io.h"
#include "utils.h"
#include "atom_param.h"
#include "cube.h"
#include "molmanipul.h"
#include "trajanal.h"
#include "time.h"
#include <xdrfile.h>
#include <xdrfile_xtc.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include "errors.h"

int tstart;
int tstop;

void tanalize ( input_t * inppar )
{
    int snapsize;
    XDRFILE * xd_read;
    FILE * fxmol;
    // FILE * frepxyz;
    FILE * fsdist;
    // FILE * fxyz;
#ifdef DEBUG
    // FILE * fsxyzlo;
    // FILE * fsxyzup;
#endif

    int i;
    // int n[DIM];
    int nref;
    int natoms;
    int nmask;

    int * mask;
    int * refmask;

    atom_t * atoms;

    i = 0;
    nref = 0;
    nmask = 0;

    if ( !( inppar->periodic ) )
        printf("W A R N I N G: calculation proceeds w/o periodic boundary conditions. Often this is not what you want and Frank always forgot to set 'periodic' in input\n");

    /* read initial snapshot to get structure */

    char text[MAXSTRLEN];
    
    if ( inppar->xdrread ) {
        int result_xtc;
        int natoms_xtc;

        fxmol = fopen(&inppar->structure[0], "r");
        natoms = atoi( fgets ( text, MAXSTRLEN, fxmol ) );
        read_xmol(&inppar->structure[0], &atoms);

#ifdef XDRCAP
        xd_read = xdrfile_open ( &inppar->trajectory[0], "r" );
        result_xtc = read_xtc_natoms( &inppar->trajectory[0], &natoms_xtc);
#endif

        if ( exdrOK != result_xtc ) {
            printf("Something went wrong opening XDR file\n"); // Error
            exit ( 1 );
        }

        if ( natoms_xtc != natoms ) {
            printf("XDR file and xyz file do not contain the same number of atoms\nNot continuing\n");
            exit ( 1 );
        }
            
    }
    else {
        fxmol = fopen(&inppar->trajectory[0], "r");
        natoms = atoi( fgets ( text, MAXSTRLEN, fxmol ) );
        snapsize = xmol_snap_bytesize(fxmol);
        read_xmol(inppar->trajectory, &atoms);
    }

    mask = get_mask(inppar->maskkind, inppar->mask, inppar->nkinds, atoms, natoms);

    if ( strstr ( inppar->refmask, EMPTY ) == NULL )
        refmask = get_mask(inppar->refmaskkind, inppar->refmask, inppar->refnkinds, atoms, natoms);
    else {
        printf("Cannot do analysis without 'refmask'\n");
        exit ( 1 );
    }

    nref = 0;
    while ( refmask[nref] != -1 )
        nref++;

    if ( !(nref) ) {
        printf("Cannot continue with 0 reference atoms\n");
        exit ( 1 );
    }

    atom_t refatoms[nref];

    nmask = 0;
    while ( mask[nmask] != -1 )
        nmask++;

    for ( i=0; i<nref; i++ )
        refatoms[i] = atoms[refmask[i]];

    // real origin[DIM];
    // real boxv[DIM][DIM];

    // for (i=0; i<DIM; i++) {
    //     origin[i] = ZERO;
    //     n[i] = inppar->pbc[i] / inppar->resolution;

    //     for ( j=0; j<DIM; j++ )
    //         boxv[i][j] = ZERO;

    //     boxv[i][i] = inppar->pbc[i] / n[i];
    // }

    int counter = 0;

    int ntot = ( inppar->stop - inppar->start ) / inppar->stride;
    int frwrd = inppar->start + 1;
    char * htw = "w";

    for ( i=inppar->start; i<inppar->stop; i += inppar->stride )
    {
        /* read snapshot */
        if ( inppar->xdrread ) {
            read_xtr_forward ( xd_read, frwrd, atoms, natoms );
            frwrd = inppar->stride;
        }
        else {
            xmolreader(fxmol, snapsize, i, atoms, natoms);
        }

        if ( inppar->tasknum == SURFDIST ) {

            // FU| check here, still need surface area determination

            cube_t surface;
            int fake_n[DIM];
            real fake_origin[DIM];
            real fake_boxv[DIM][DIM];
            real disthi, distlo;
            real **surf_2d_up, **surf_2d_down;
            int * surf_up_inds, * surf_down_inds;
            int inthi, intlo;
            char tmp[MAXSTRLEN];

            char opref[MAXSTRLEN];
            sprintf(opref, "%s%i_", inppar->outputprefix, i);

            surface = instant_surface_periodic ( mask, atoms, natoms, inppar->zeta, inppar->surfacecutoff, inppar->output, opref, inppar->pbc, inppar->resolution, inppar->accuracy, 0, fake_origin, fake_n, fake_boxv, inppar->periodic, 0 );

            surf_2d_up = allocate_matrix_real_2d ( surface.n[0], surface.n[1] );
            surf_2d_down = allocate_matrix_real_2d ( surface.n[0], surface.n[1] );

            /* check here, and remove hard-coded surface direction */
            int direction = 2;
            int newsurf = 0;

            get_2d_representation_ils ( &surface, surf_2d_up, surf_2d_down, inppar->surfacecutoff, newsurf, surf_up_inds, surf_down_inds, direction );

#ifdef DEBUG
            FILE *fsxyzlo, *fsxyzup;

            sprintf(tmp, "%s%i_%s", inppar->outputprefix, i, "atrep_surflo.xyz");
            fsxyzlo = fopen(&tmp[0], "w");;

            sprintf(tmp, "%s%i_%s", inppar->outputprefix, i, "atrep_surfup.xyz");
            fsxyzup = fopen(&tmp[0], "w");;

            fprintf ( fsxyzlo, "%i\n\n", surface.n[0]*surface.n[1] );
            fprintf ( fsxyzup, "%i\n\n", surface.n[0]*surface.n[1] );

            int g, h;

            for ( g=0; g<surface.n[0]; g++ )
                for ( h=0; h<surface.n[1]; h++ ) {
                    fprintf ( fsxyzlo, "S %21.10f %21.10f %21.10f\n", BOHR * g * surface.boxv[0][0], BOHR * h * surface.boxv[1][1], BOHR * surf_2d_down[g][h] );
                    fprintf ( fsxyzup, "S %21.10f %21.10f %21.10f\n", BOHR * g * surface.boxv[0][0], BOHR * h * surface.boxv[1][1], BOHR * surf_2d_up[g][h] );
            }

            fclose ( fsxyzlo );
            fclose ( fsxyzup );
#endif

            // check here, this needs to be done for all of the solute atoms, not just assume that there is only one
            get_distance_to_surface ( &disthi, &distlo, &inthi, &intlo, &surface, surf_2d_up, surf_2d_down, atoms, refmask, nref, natoms, inppar->pbc, inppar->output, opref, 2, inppar->surfacecutoff );

#ifdef DEBUG
            char funame[MAXSTRLEN];
            sprintf ( funame, "%s%s.dat", opref, "2dsurfup" );
            write_matrix_real_2d_to_file_w_cont_spacing ( funame, surf_2d_up, surface.n[0], surface.n[1], &(surface.origin[0]), surface.boxv[0][0], surface.boxv[1][1] );
             sprintf ( funame, "%s%s.dat", opref, "2dsurfdown" );
             write_matrix_real_2d_to_file_w_cont_spacing ( funame, surf_2d_down, surface.n[0], surface.n[1], &(surface.origin[0]), surface.boxv[0][0], surface.boxv[1][1] );
#endif

            free_matrix_real_2d ( surf_2d_up, surface.n[0] );
            free_matrix_real_2d ( surf_2d_down, surface.n[0] );

            /* check here, and move stuff for refinement box creation somewhere else */
        
            sprintf(tmp, "%s%s", inppar->outputprefix, "surfdist.dat");
            fsdist = fopen(&tmp[0], htw);

            if ( strncmp ( htw, "w", 1 ) == 0 ) {
                fprintf ( fsdist, "#            lower                 upper\n");
            }

            fprintf ( fsdist, "%21.10f %21.10f\n", distlo, disthi);
            fclose ( fsdist );
            htw = "a";

            free ( surface.atoms );
            free ( surface.voxels );
        }

    }
    printf("%4.2f %% done\n", (real) counter / ntot * 100.);


    if ( inppar->output ) {
    }

    free(mask);
    free(refmask);
    free(atoms);

#ifdef XDRCAP
    if ( inppar->xdrread )
        xdrfile_close ( xd_read );
#endif

    return;
}