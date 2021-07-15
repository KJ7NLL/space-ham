/* sgp0.c-- a part of the SGP - C Library
 *
 * Copyright (c) 1999-2002 Dominik Brodowski
 *           (c) 1992-2000 Dr TS Kelso
 *
 * This file is part of the SGP C Library.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public 
 * License as published by the Free Software Foundation; either 
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU 
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public 
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 *
 * This SGP C Library is based on the SGP4 Pascal Library by Dr TS Kelso.
 *
 * You can reach Dominik Brodowski by electronic mail at
 * mail@brodo.de
 * and by paper mail at 
 * Karlstrasse 11a; 72072 Tuebingen; Germany
 */



/* ------------------------------------------------------------------- *
 * ----------------------------- includes ---------------------------- *
 * ------------------------------------------------------------------- */

#include <math.h>
#include "sgp.h"
#include "sgp_int.h"



/* ------------------------------------------------------------------- *
 * ------------------------------- sgp8 ------------------------------ *
 * ------------------------------------------------------------------- */

void sgp0 (double tsince, struct vector *pos, struct vector *vel, struct sgp_data *satdata)
{
	double a1=0, cosio=0, sinio=0, sigma1=0, a0=0, p0=0;
	double q0=0, L0=0, xnodedt=0, domegadt=0, temp=0, a=0;
	double e=0, p=0, xnodeso=0, omegaso=0, Ls=0, Aynsl=0;
	double Axnsl=0, L=0, u=0, deltaEw=0, temp2=0, temp3=0;
	double Ew=0, ecose=0, esine=0, elsqr=0, pl=0, r=0;
	double rdot=0, rvdot=0, sinu=0, cosu=0, ik=0, rk=0;
	double uk=0, xnodek=0, xke=0;
	int i;
	struct vector MV, NV, UV, VV;

	/* Constants */
	xke = sqrt(((3600.0 * ge) / cube(xkmper)));
	temp2 = xke / (satdata->xno);
	a1 = pow (temp2 , _tothrd);
	cosio = cos (satdata->xincl);
	sinio = sin (satdata->xincl);
	sigma1 = 1.5 * CK2 * ( 3.0 * (sqr(cosio) ) - 1.0 ) / (sqr(a1) * pow( 1.0 - sqr(satdata->eo) , 1.5));
	a0 = a1 * (1.0 - (1.0 / 3.0) * sigma1 - sqr( sigma1 )- ( 134.0 / 81.0 ) * sqr(sigma1) * sigma1);
	p0 = a0 * (1.0 - sqr(satdata->eo));
	q0 = a0 * (1.0 - (satdata->eo));
	L0 = (satdata->xmo) + (satdata->omegao) + (satdata->xnodeo);
	xnodedt = -3.0 * (CK2) * ((satdata->xno) * cosio ) / (sqr(p0));
	domegadt = 1.5 * CK2 * ( (satdata->xno) * (5.0 * sqr(cosio) - 1.0)) / (sqr(p0));

	/*  Secular effect of atmospheric drag and gravitation */
	temp = (satdata->xno) + 2.0 * (satdata->xndt2o) * tsince + 3.0 * (satdata->xndd6o) * sqr(tsince);
	temp = (satdata->xno)/temp;
	a = a0 * pow(temp , _tothrd);
	if (a > q0) 
		e = (1.0 - (q0 / a));
	else
		e = _e6a;
	p = a * ( 1.0 - sqr(e) );
	xnodeso = (satdata->xnodeo) + xnodedt * tsince;
	omegaso = (satdata->omegao) + domegadt * tsince;
	Ls = L0 + ((satdata->xno) + domegadt + xnodedt) * tsince + (satdata->xndt2o) * sqr(tsince) + (satdata->xndd6o) * sqr(tsince)  * tsince;

	/* Long-period periodics */
	Aynsl = e * sin(omegaso) - (XJ3 * sinio) / (4.0 * CK2 * p);
	Axnsl = e * cos(omegaso); 
	temp2 = (XJ3 * Axnsl * sinio * (3.0 + 5.0 * cosio));
	temp3 = 8.0 * CK2 * p * (1.0 + cosio);
	temp = Ls - temp2 / temp3;
	L = fmod2p( temp );

	/* Solve Kepler's eqation */
	u = fmod2p(L - xnodeso);
	temp = u;
	i = 1;
	do {
		temp2 = u - Aynsl * cos(temp) + Axnsl * sin(temp) - temp;
		temp3 = 1.0 - Aynsl * sin(temp) - Axnsl * cos(temp);
		deltaEw = temp2 / temp3;
		Ew = deltaEw + temp;
		temp2 = temp;
		temp = Ew;
		i++;
        } while ((i<=10) && (fabs(Ew-temp2) > _e6a));
	/* intermediate quantities */
	ecose = Axnsl * cos(Ew) + Aynsl * sin(Ew);
	esine = Axnsl * sin(Ew) - Aynsl * cos(Ew);
	elsqr = sqr(Axnsl) + sqr(Aynsl);
	pl = a * (1.0 - elsqr);
	r = a * (1.0 - ecose);
	rdot = xke * sqrt(a)* esine/r;
	rvdot = xke * sqrt(pl)/r;
	sinu = (a / r) * (sin(Ew) - Aynsl - ( (Axnsl*esine) / (1.0 + sqrt(1.0 - elsqr))));
	cosu = (a / r) * (cos(Ew) - Axnsl + ( (Aynsl*esine) / (1.0 + sqrt(1.0 - elsqr))));
	u = actan (sinu, cosu);

	/* short-period perturbations */
	rk = r + (1.0/2.0) * CK2 * (sqr(sinio)/pl) * cos(2.0 * u);
	uk = u - (1.0/4.0) * CK2 * (sin(2.0*u) * (7.0 * sqr(cosio) - 1.0)) / sqr(pl);
	xnodek = xnodeso + 1.5 * CK2 * (cosio * sin(2.0 * u)) / sqr(pl);
	ik = (satdata->xincl) + 1.5 * CK2 * (sinio * cosio * cos(2.0 * u)) / sqr(pl);
	MV.v[0] = -sin( xnodek ) * cos(ik);
	MV.v[1] = cos( xnodek )  * cos(ik);
	MV.v[2] = sin( ik );
	NV.v[0] = cos( xnodek );
	NV.v[1] = sin( xnodek );
	NV.v[2] = 0;
	for (i=0;(i<3);i++)
	{
		UV.v[i] = MV.v[i] * sin(uk) + NV.v[i] * cos(uk);
		VV.v[i] = MV.v[i] * cos(uk) - NV.v[i] * sin(uk);
        }

        /* position + velocity */
	for (i=0;(i<3);i++)
	{
		pos->v[i] = rk * UV.v[i];
		vel->v[i] = rdot * UV.v[i] + rvdot * VV.v[i];
        }
}



/* ------------------------------------------------------------------- *
 * ----------------------------- sgp0call ---------------------------- *
 * ------------------------------------------------------------------- */

void sgp0call (double time, struct vector *pos, struct vector *vel, struct sgp_data *satdata)
{
	double tsince;
	tsince = ( time - (satdata->julian_epoch) ) * _xmnpda;
	sgp0 (tsince, pos, vel, satdata);
}


