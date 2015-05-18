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
#include "surf.h"
#include "molmanipul.h"
#include "trajanal.h"
#include "time.h"
#include <xdrfile/xdrfile.h>
#include <xdrfile/xdrfile_xtc.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include "errors.h"

int tstart;
int tstop;

int tanalize ( input_t * inppar )
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
    real ntotarea = ZERO;
    real ntotvol = ZERO;

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

#ifdef HAVE_XDRFILE
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

        fclose ( fxmol );

    }
    else {
        fxmol = fopen(&inppar->trajectory[0], "r");
        natoms = atoi( fgets ( text, MAXSTRLEN, fxmol ) );
        snapsize = xmol_snap_bytesize(fxmol);
        read_xmol(inppar->trajectory, &atoms);
    }

    nmask = get_mask(&(mask), inppar->maskkind, inppar->mask, inppar->nkinds, atoms, natoms);

    if  (inppar->tasknum == SURFDENSPROF ) {
            if ( ( strstr ( inppar->refmask, EMPTY ) != NULL ) && ( inppar->nofrags ) )  {
                print_error ( MISSING_INPUT_PARAM, "refmask or fragments" );
                exit ( MISSING_INPUT_PARAM );
            } else if ( ( strstr ( inppar->refmask, EMPTY ) == NULL ) && ( ! ( inppar->nofrags ) ) )  {
                print_error ( CONFLICTING_OPTIONS, "refmask or fragments" );
                exit ( CONFLICTING_OPTIONS );
            }
    }

    int * frags[inppar->numfrags];
    int * frag;
    int ntotfrag = 0;
    int o;
    char buff[10] = "indices";

    if ( inppar->nofrags ) {
        printf("Using indices given in 'refmask'\n");
        nref = get_mask(&refmask, inppar->refmaskkind, inppar->refmask, inppar->refnkinds, atoms, natoms);

        if ( !(nref) ) {
            printf("Cannot continue with 0 reference atoms\n");
            exit ( 1 );
        }
    }
    else {

        /* check here and wrap fragments to central box */

        printf("Using fragments given in 'fragments'\n");

        for ( o=0; o<inppar->numfrags; o++ ) {
            // int tmpfrg  = get_mask(&frag, &(buff[0]), inppar->fragments[o], inppar->natomsfrag[o], atoms, natoms);
            // check here, and change this later, because it might get ugly with memory freeing
            frags[o] = inppar->fragments[o];
            ntotfrag += inppar->natomsfrag[o];
        }
    }

    real *densprof;
    real mxdim;
    int ndprof;
    real drdprof = inppar->profileres;
    real dv;

    if ( inppar->tasknum == SURFDENSPROF ) {

        int mini;

        if ( ! ( inppar->pbcset ) ) {
             print_error ( MISSING_INPUT_PARAM, "pbc" );
             return MISSING_INPUT_PARAM;
        }

        // mxdim = find_maximum_1d_real ( &mini, inppar->pbc, DIM );
        // just use the information given by the 'direction' keyword

        mxdim = ZERO;
        for ( i=0; i<DIM; i++ )
            mxdim += sqr ( inppar->pbc[i] );

        mxdim = sqrt ( mxdim );

        // we want to use both above and below surface
        ndprof = 2 * ( int ) ( mxdim / inppar->profileres );

        densprof = ( real * ) calloc ( ndprof, sizeof(real) );

    }

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
    real *dx;

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

        if ( ( inppar->tasknum == SURFDIST ) || ( inppar->tasknum == SURFDENSPROF ) ) {

            // FU| check here, still need surface area determination

            cube_t surface;
            int fake_n[DIM];
            real fake_origin[DIM];
            real fake_boxv[DIM][DIM];
            real disthi, distlo;
            int * surf_inds;
            real vol;
            int inthi, intlo;
            char tmp[MAXSTRLEN];

            char opref[MAXSTRLEN];
            sprintf(opref, "%s%i_", inppar->outputprefix, i);

            surface = instant_surface_periodic ( mask, atoms, natoms, inppar->zeta, inppar->surfacecutoff, inppar->output, opref, inppar->pbc, inppar->resolution, inppar->accuracy, 0, fake_origin, fake_n, fake_boxv, inppar->periodic, 0 );

            if ( inppar->postinterpolate > 1 ) {
                if ( ! ( inppar->localsurfint ) ) {
                    cube_t fine;

                    if ( inppar->interpolkind == INTERPOLATE_TRILINEAR )
                        fine = interpolate_cube_trilinear ( &surface, inppar->postinterpolate, inppar->periodic );
#ifdef HAVE_EINSPLINE
                    else if ( inppar->interpolkind == INTERPOLATE_BSPLINES )
                        fine = interpolate_cube_bsplines ( &surface, inppar->postinterpolate, inppar->periodic );
#endif

                    free ( surface.atoms );
                    free ( surface.voxels );

                    surface = fine;

                    surface.atoms = fine.atoms;
                    surface.voxels = fine.voxels;

                    if ( inppar->output > 2 ) {
                        sprintf(tmp, "%s%i_%s", inppar->outputprefix, i, "interpolated-instant-surface.cube");
                        write_cubefile(tmp, &surface);
                    }
                }
            }

            if ( ( inppar->normalization == NORM_BULK) || ( inppar->normalization == NORM_SLAB ) ) {
                vol = get_bulk_volume ( &surface, inppar->surfacecutoff );

                if ( inppar->normalization == NORM_SLAB )
                    vol = inppar->pbc[0]*inppar->pbc[1]*inppar->pbc[2] - vol;
            }
            else
                vol = inppar->pbc[0]*inppar->pbc[1]*inppar->pbc[2];

            // check here, depending on whether we want to look at stuff in the non-solvent phase or in the solvent phase we need to take different volumes
            // printf("%21.10f%21.10f\n", vol, inppar->pbc[0]*inppar->pbc[1]*inppar->pbc[2]);

            ntotvol += vol;

            /* check here, and remove hard-coded surface direction */
            int * direction;
            real * grad;
            int newsurf = 0;
            int nsurf = 0;

            real area = ZERO;
            real ** surfpts;

            surfpts = get_2d_representation_ils ( &nsurf, &direction, &grad, &surface, inppar->surfacecutoff, newsurf, surf_inds, inppar->direction, &area, inppar->periodic );

            ntotarea += area;

            // use function write_combined_xmol
            if ( inppar->surfxyz ) {
                FILE *fsxyzal;

                sprintf(tmp, "%s%i_%s", inppar->outputprefix, i, "atrep_surface.xyz");
                fsxyzal = fopen(&tmp[0], "w");;

                int a, g, k;

                fprintf ( fsxyzal, "%i\n\n", nsurf );

                for ( g=0; g<nsurf; g++ ) {
                    fprintf(fsxyzal, "%5s", "X");
                    for ( k=0; k<DIM; k++ ) {
                        fprintf ( fsxyzal, "    %21.10f", BOHR * surfpts[g][k]);
                    }
                    fprintf( fsxyzal, "\n" );
                }

                fclose ( fsxyzal );
            }

            if ( inppar->tasknum == SURFDENSPROF ) {

                int r;
                signed int ind;
                int hndprof = ndprof / 2.;
                int *fakemask;
                int fakenum;

                if ( counter == 0 )
                    dx = get_box_volels ( &surface );

                int nfrg;

                if ( inppar->nofrags )
                    nfrg = nref;
                else
                    nfrg = inppar->numfrags;

                real * dstnc = ( real * ) malloc ( nfrg * sizeof ( real ) );

#ifdef OPENMP
                // each thread should have about the same amount of work, so the atomic update will not be too harmful
#pragma omp parallel for default(none) \
                private(r,fakemask,fakenum,ind) \
                shared(dstnc,nfrg,refmask,frags,inppar,surface,direction,natoms,opref,densprof,hndprof,atoms,nsurf,grad,surfpts,dx)
#endif
                for ( r=0; r<nfrg; r++ ) {

                    if ( inppar->nofrags ) {
                        fakemask = &(refmask[r]);
                        fakenum = 1;
                    }
                    else {
                        fakemask = frags[r];
                        fakenum = inppar->natomsfrag[r];
                    }

                    int mnnd;
                    dstnc[r] = get_distance_to_surface ( &mnnd, &surface, nsurf, surfpts, direction, grad, atoms, fakemask, fakenum, natoms, inppar->pbc, inppar->output, opref, inppar->surfacecutoff, inppar->periodic );

                    if ( ( inppar->postinterpolate > 1 ) && ( inppar->localsurfint ) && ( fabs ( dstnc[r] ) < inppar->ldst ) ) {

                        // need index of point on surface
                        // this is mnnd

                        // find which voxel that point belongs to

                        int ix[DIM];
                        int ivx;

                        get_index_triple ( ix, surfpts[mnnd], inppar->pbc, surface.origin, surface.n, dx, inppar->periodic );
                        ivx = get_index ( surface.n, ix[0], ix[1], ix[2] );

                        // create fake box around that point
                        real origin[DIM];
                        //real cboxv[DIM][DIM];
                        int cn[DIM];

                        int l, m, n;

                        for ( l=0; l<DIM; l++ ) {
                            origin[l] = surface.voxels[ivx].coords[l] - inppar->lint * dx[l];
                            cn[l] = ( inppar->lint*2 + 1 );

                            // printf("%21.10f\n", origin[l] / dx[l]);
                            // for ( m=0; m<DIM; m++ )
                            //     printf("%21.10f", surface.boxv[l][m]);
                            // printf("\n");
                        }

                        cube_t cutcube, fine;
                        cutcube = initialize_cube ( origin, surface.boxv, cn, surface.atoms, surface.natoms );

                        // printf("%21.10f %21.10f %21.10f\n", cutcube.boxv[0][0], cutcube.boxv[1][1], cutcube.boxv[2][2]);

                        // assign the data from the original cube file into small cutout

                        int mnx, mxx, mny, mxy, mnz, mxz;

                        // check here, this was just a quick work-around
                        mnx = ix[0] - inppar->lint;
                        mxx = ix[0] + inppar->lint + 1;

                        mny = ix[1] - inppar->lint;
                        mxy = ix[1] + inppar->lint + 1;

                        mnz = ix[2] - inppar->lint;
                        mxz = ix[2] + inppar->lint + 1;

                        int oindx, nindx;

                        // printf("%i %i %i %i %i %i\n", mnx, mxx, mny, mxy, mnz, mxz);

                        int cnl = 0;
                        int cnm = 0;
                        int cnn = 0;

                        int il, im, in;

                        cnl = 0;
                        for ( l=mnx; l<mxx; l++ ) {
                            cnm = 0;
                            for ( m=mny; m<mxy; m++ ) {
                                cnn = 0;
                                for ( n=mnz; n<mxz; n++ ) {
                                    periodify_indices ( &il, &(surface.n[0]), &l, 1 );
                                    periodify_indices ( &im, &(surface.n[1]), &m, 1 );
                                    periodify_indices ( &in, &(surface.n[2]), &n, 1 );

                                    oindx = get_index ( surface.n, il, im, in );
                                    nindx = get_index ( cutcube.n, cnl, cnm, cnn );

                                    // printf("%5i %5i %5i %5i %5i %5i\n", l, m, n, cnl, cnm, cnn);
                                    cutcube.voxels[nindx].data = surface.voxels[oindx].data;

                                    cnn++;
                                }
                                cnm++;
                            }
                            cnl++;
                        }
                        // printf("%21.10f %21.10f %21.10f\n", cutcube.origin[0], cutcube.origin[1], cutcube.origin[2]);

#ifdef DEBUG
                        char tmp[MAXSTRLEN];
                        sprintf(tmp, "%s%i_%s", inppar->outputprefix, r, "test_local-non-interpolation.cube");
                        write_cubefile(tmp, &cutcube);
#endif

                        // printf("%21.10f %21.10f %21.10f\n", cutcube.origin[0], cutcube.origin[1], cutcube.origin[2]);

                        // interpolate in that region

                        if ( inppar->interpolkind == INTERPOLATE_TRILINEAR )
                            fine = interpolate_cube_trilinear ( &cutcube, inppar->postinterpolate, 0 );
#ifdef HAVE_EINSPLINE
                        else if ( inppar->interpolkind == INTERPOLATE_BSPLINES )
                            fine = interpolate_cube_bsplines ( &cutcube, inppar->postinterpolate, 0 );
#endif

#ifdef DEBUG
                        sprintf(tmp, "%s%i_%s", inppar->outputprefix, r, "test_local-interpolation.cube");
                        write_cubefile(tmp, &fine);
                        // printf("%21.10f %21.10f %21.10f\n\n", fine.origin[0], fine.origin[1], fine.origin[2]);
#endif

                        free ( cutcube.atoms );
                        free ( cutcube.voxels );

                        // get_2d_representation

                        int * drctn;
                        real * grd;
                        int nwsrf = 0;
                        int nsrf = 0;
                        int * srf_nds;

                        real rea = ZERO;
                        real ** srfpts;

                        srfpts = get_2d_representation_ils ( &nsrf, &drctn, &grd, &fine, inppar->surfacecutoff, nwsrf, srf_nds, inppar->direction, &rea, 0 );
                        // printf("previous number of surface points: %i\n", nsrf);

#ifdef DEBUG
                        if ( inppar->surfxyz ) {
                            FILE *fsxyzal;

                            sprintf(tmp, "%s%i_%s", inppar->outputprefix, r, "test_atrep_surface.xyz");
                            fsxyzal = fopen(&tmp[0], "w");;

                            int a, g, k;

                            fprintf ( fsxyzal, "%i\n\n", nsrf );

                            for ( g=0; g<nsrf; g++ ) {
                                fprintf(fsxyzal, "%5s", "X");
                                for ( k=0; k<DIM; k++ ) {
                                    fprintf ( fsxyzal, "    %21.10f", BOHR * srfpts[g][k]);
                                }
                                fprintf( fsxyzal, "\n" );
                            }

                            fclose ( fsxyzal );
                        }
#endif

                        // get distance again

                        int mnnd;
                        // printf("outdist: %21.10f", dstnc[r]);
                        dstnc[r] = get_distance_to_surface ( &mnnd, &fine, nsrf, srfpts, drctn, grd, atoms, fakemask, fakenum, natoms, inppar->pbc, inppar->output, opref, inppar->surfacecutoff, inppar->periodic );
                        // printf("%21.10f, number of surface points %i\n", dstnc[r], nsrf);

                        for ( l=0; l<nsrf; l++ )
                            free ( srfpts[l] );

                        if ( nwsrf )
                            free ( srf_nds );

                        free ( grd );
                        free ( srfpts );
                        free ( drctn );

                        free ( fine.atoms );
                        free ( fine.voxels );
                    }

                    ind = ( int ) floor ( dstnc[r] / inppar->profileres );

#pragma omp atomic update
                    densprof[ hndprof + ind ] += 1.; // / nsurf;

                }

                if ( inppar->output ) {
                    sprintf(tmp, "%s%i_%s", inppar->outputprefix, i, "surfdist.dat");
                    fsdist = fopen(&tmp[0], "w");

                    fprintf ( fsdist, "#            index              distance\n");
                    for ( r=0; r<nfrg; r++ )
                        fprintf ( fsdist, "%21i %21.10f\n", r, dstnc[r]);

                    fclose ( fsdist );
                }

            }

            /* check here, and move stuff for refinement box creation somewhere else */

            int k;
            for ( k=0; k<nsurf; k++ )
                free ( surfpts[k] );

            if ( newsurf )
                free ( surf_inds );

            free ( grad );
            free ( surfpts );
            free ( direction );
            free ( surface.atoms );
            free ( surface.voxels );
        }

        printf("%4.2f %% done\r", (real) counter / ntot * 100.);
        fflush(stdout);

        counter++;

    }
    printf("%4.2f %% done\n", (real) counter / ntot * 100.);

    if ( ( inppar->tasknum == SURFDENSPROF ) && ( inppar->output ) ) {

        int natdens;
        int navsurf;
        real avarea;
        real avvol;
        real factor, partdens, norm;

        if ( inppar->nofrags )
            natdens = nref;
        else
            natdens = inppar->numfrags;

        real smarea;
        if ( inppar->periodic ) {
            real fctr;

            avarea = ntotarea / counter;
            avvol = ntotvol / counter;
            smarea = avarea;

            partdens = (real) natdens / ( avvol );
            // partdens = (real) natdens / ( inppar->pbc[0] * inppar->pbc[1] * inppar->pbc[2]);

            factor = (real) counter * drdprof * smarea * partdens;

        }
        else {
            factor = (real) counter * drdprof * smarea * natdens;
        }

        printf("particle density:     %21.10f\n", partdens);
        printf("normalization factor: %21.10f\n", factor);
        printf("average surface area: %21.10f\n", smarea);

        FILE *fdprof;
        char tmp[MAXSTRLEN];

        sprintf(tmp, "%s%s", inppar->outputprefix, "densprof.dat");
        fdprof = fopen(&tmp[0], "w");

        int hndprof = ndprof / 2;
        real hdrdprof = drdprof / 2.;
        for ( i=-hndprof; i<hndprof; i++ ) {

            norm = factor;

            fprintf ( fdprof, "%21.10f %21.10f %21.10f\n", BOHR*i*drdprof+hdrdprof, densprof[i+hndprof], densprof[i+hndprof] / norm);
        }

        fclose ( fdprof );

    }

    if ( ! ( inppar->nofrags ) ) {
        for ( i=0; i<inppar->numfrags; i++ ) {
            // free(inppar->fragments[i]);
            free(frags[i]);
        }

        free(inppar->natomsfrag);
    }
    else {
        free ( refmask );
    }

    if ( inppar->tasknum == SURFDENSPROF ) {
        free ( densprof );
        free ( dx );
    }

    free(mask);
    // free(refmask);
    free(atoms);

    if ( ! ( inppar->xdrread ) )
        fclose ( fxmol );
#ifdef HAVE_XDRFILE
    else if ( inppar->xdrread )
        xdrfile_close ( xd_read );
#endif

    return 0;
}
