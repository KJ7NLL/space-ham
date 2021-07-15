/* sgp8sdp8.c -- a part of the SGP - C Library
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

void sgp8 (double tsince, struct vector *pos, struct vector *vel, struct sgp_data *satdata)
{
	struct vector UV, VV;
	int i;
	double a1=0, cosio=0, sinio=0, delta1=0, a0=0, delta0=0, aoii=0, noii=0, B=0;
	double rho=0, beta=0, theta=0, M1dot=0, omega1dot=0, xnode1dot=0, M2dot=0, omega2dot=0;
	double xnode2dot, ldot=0, omegadot=0, xnodedot=0;
	double tsi=0, temp=0, eta=0, phi=0, alpha=0, C0=0, C1=0, D1=0, D2=0, D3=0, D4=0;
	double D5=0, B1=0, B2=0, B3=0, A30=0, C2=0, C3=0, n0dot=0, C4=0, C5=0, D6=0, D7=0;
	double D8=0, e0dot=0, etadot=0, alphadottoalpha=0, C0dottoC0=0, tsidottotsi=0;
	double C1dottoC1=0, phidottophi=0, C6=0,D9=0, D10=0, D11=0, D12=0, D13=0, D14=0;
	double D15=0, D1dot=0, D2dot=0, D3dot=0, D4dot=0, D5dot=0, C2dot=0, C3dot=0, D16=0;
	double n0ddot=0, e0ddot=0, D17=0, etaddot=0, D18=0, D19=0, D1ddot=0, tsiddottotsi=0;
	double n0tdot=0, p=0, gamma=0, nd=0, q=0, ed=0, n=0, edot=0, e=0, Z1=0, omega=0, xnode=0;
	double M=0, Ew=0, deltaEw=0, a=0, sinf=0, cosf=0, rii=0;
	double deltaR=0, deltaU=0, r=0, rdot=0, rfdot=0;
	double y4=0, y5=0, lambda=0, deltaI=0, f=0, cosI2=0, sinu=0, cosu=0, smalln0ddot=0;
	double xke=0, s=0, qo=0, qoms2t=0, temp2=0;
        /* Constants */
	rho = (2.461 * 0.00001 * xkmper); 
	s = _ae + 78 / xkmper;
	qo = _ae + 120 / xkmper;
	xke = sqrt((3600.0 * ge) / cube(xkmper));
	qoms2t = sqr ( sqr (qo - s));
	temp2 = xke / (satdata->xno);
	a1 = pow (temp2 , _tothrd);
	cosio = cos(satdata->xincl);
	sinio = sin(satdata->xincl);
	delta1 = 1.5 * CK2 * (3.0 * cosio * cosio - 1.0) / (sqr(a1) * pow((1.0 - sqr(satdata->eo)), 1.5));
	a0 = a1 * (1.0 - (1.0/3.0) * delta1 - sqr(delta1) - (134.0/81.0) * sqr(delta1) * delta1);
	delta0 = 1.5 * CK2 * (3.0 * cosio * cosio - 1.0) / (sqr(a0) * pow((1.0 - sqr(satdata->eo)), 1.5));
	noii = satdata->xno / (1.0 + delta0);
	aoii = a0 / (1.0 - delta0);
	/*B term*/
	B = 2.0 * (satdata->bstar) / rho;
	/*constants II*/
	beta = sqrt(1.0 - sqr(satdata->eo));
	theta = cosio;
	temp = (noii * CK2) / (sqr(aoii) * sqr(beta) * beta);
	M1dot = -1.5 * temp * (1.0 - 3.0 * sqr(theta));
	omega1dot = -1.5 * (temp / beta) * (1.0 - 5.0*sqr(theta));
	xnode1dot = -3.0 * (temp / beta) * theta;
	M2dot = (3.0/16.0) * (temp * CK2 / (sqr(aoii) * pow(beta, 4.0))) * (13.0 - 78.0 * sqr(theta) + 137.0 * pow(theta, 4.0));
	omega2dot = (3.0/16.0) * (temp * CK2 / (sqr(aoii) * pow(beta, 5.0))) * (7.0 - 114.0 *sqr(theta)+ 395.0 * pow(theta,4)) + 1.25 * (noii * CK4) / (pow(aoii, 4.0) * pow(beta, 8.0))* (3.0 - 36.0 * sqr(theta) + 49.0 * pow(theta,4));
	xnode2dot = 1.5 * (temp * CK2 / (sqr(aoii) * pow(beta, 5.0))) * theta * (4.0 - 19.0 * sqr(theta))+ 2.5 * (noii * CK4) / (pow(aoii, 4.0) * pow(beta, 8.0)) * theta * (3.0 - 7.0*sqr(theta));
	ldot = noii + M1dot + M2dot;
	omegadot = omega1dot + omega2dot;
	xnodedot = xnode1dot + xnode2dot;
	tsi = 1.0/(aoii * sqr(beta) - s);
	eta = satdata->eo * s * tsi;
	phi = sqrt(1.0 - sqr(eta));
	alpha = sqrt(1.0 + sqr(satdata->eo));
	C0 = 0.5 * B * rho * qoms2t * noii * aoii * pow(tsi, 4.0) / (alpha * pow(phi, 7.0));
	C1 = 1.5 * noii * pow(alpha, 4.0) * C0;
	D1 = (tsi * (1.0 / sqr(phi))) / (aoii * sqr(beta));
	D2 = 12.0 + 36.0 * sqr(eta) + 4.5 * pow(eta, 4.0);
	D3 = 15.0 * sqr(eta) + 2.5 * pow(eta, 4.0);
	D4 = 5.0 * eta + (15.0 / 4.0) * pow(eta, 3.0);
	D5 = tsi / sqr(phi);
	B1 = -CK2 * (1.0 - 3.0 * sqr(theta));
	B2 = -CK2 * (1.0 - sqr(theta));
	A30 = -XJ3 * pow(_ae, 3.0) / CK2;
	B3 = A30 * sinio;
	C2 = D1 * D3 * B2;
	C3 = D4 * D5 * B3;
	n0dot = C1 * ((2.0 + sqr(eta) * (3.0 + 34.0 * sqr(satdata->eo)) + 5.0 * (satdata->eo) * (eta) * (4.0 + sqr(eta)) + 8.5 * sqr(satdata->eo)) + D1 * D2 * B1 + C2 * cos(2.0 * satdata->omegao) + C3 * sin(satdata->omegao));
	D6 = 30 * eta + 22.5 * pow(eta,3);
	D7 = 5.0 * eta + 12.5 * pow(eta,3);
	D8 = 1.0 + 6.75 * sqr(eta) + pow(eta,4);
	C4 = D1 * D7 * B2;
	C5 = D5 * D8 * B3;
	e0dot = -C0 * (eta * (4.0 + sqr(eta) + sqr(satdata->eo) * (15.5 + 7.0 * sqr(eta))) + satdata->eo * (5.0 + 15.0 * sqr(eta)) + D1 * D6 * B1 + C4 * cos(2.0 * satdata->omegao) + C5 * sin(satdata->omegao));
	alphadottoalpha = satdata->eo * e0dot / (sqr(alpha));
	C6 = (1.0/3.0) * (n0dot/noii);
	tsidottotsi = 2.0 * aoii * tsi * (C6 * sqr(beta) + satdata->eo * e0dot);
	etadot = (e0dot + satdata->eo * tsidottotsi) * s * tsi;
	phidottophi = -eta * etadot / sqr(phi);
	C0dottoC0 = C6 + 4.0 * tsidottotsi - alphadottoalpha - 7.0 * phidottophi;
	C1dottoC1 = n0dot / noii + 4.0 * alphadottoalpha + C0dottoC0;
	D9 = 6.0 * eta + 20.0 * satdata->eo + 15.0 * satdata->eo * sqr(eta) + 68.0 * sqr(satdata->eo) * eta;
	D10 = 20.0 * eta + 5.0 * pow(eta,3) + 17.0 * satdata->eo + 68 * (satdata->eo) * sqr(eta);
	D11 = eta * (72.0 + 18.0 * sqr(eta));
	D12 = eta * (30.0 + 10.0 * sqr(eta));
	D13 = 5.0 + 11.25*sqr(eta);
	D14 = tsidottotsi - 2.0 * phidottophi;
	D15 = 2.0 * (C6 + satdata->eo * e0dot / sqr(beta));
	D1dot = D1 * (D14 + D15);
	D2dot = etadot * D11;
	D3dot = etadot * D12;
	D4dot = etadot * D13;
	D5dot = D5 * D14;
	C2dot = B2 * (D1dot * D3 + D1 * D3dot);
	C3dot = B3 * (D5dot * D4 + D5 * D4dot);
	D16 = D9 * etadot + D10 * e0dot + B1 * (D1dot * D2 + D1 * D2dot) + C2dot * cos(2.0 * satdata->omegao) + C3dot * sin(satdata->omegao)+omega1dot * (C3 * cos(satdata->omegao) - 2 * C2 * sin(2.0 * satdata->omegao));
	n0ddot = n0dot * C1dottoC1 + C1 * D16;
	e0ddot = e0dot * C0dottoC0 - C0 * ((4.0 + 3.0 * sqr(eta) + 30.0 * satdata->eo * eta + 15.5 * sqr(satdata->eo) + 21.0 * sqr(eta) * sqr(satdata->eo)) * etadot + (5.0 + 15.0 * sqr(eta) + 31.0 * satdata->eo * eta + 14.0 * satdata->eo * pow(eta,3)) * e0dot + B1 * (D1dot * D6 + D1 * etadot * (30.0 + 67.5 * sqr(eta)))+ B2 * (D1dot * D7 + D1 * etadot * (5.0 + 37.5 * sqr(eta))) * cos(2.0 * satdata->omegao)+ B3 * (D5dot * D8 + D5 * eta * etadot * (13.5 + 4 * sqr(eta))) * sin(satdata->omegao) + omega1dot * (C5 * cos(satdata->omegao) - 2.0 * C4 * sin(2.0 * satdata->omegao)));
	D17 = n0ddot / noii - sqr(n0dot/noii);
	tsiddottotsi = 2.0 * (tsidottotsi - C6) *tsidottotsi + 2.0 * aoii * tsi * ((1.0/3.0) * D17 * sqr(beta) - 2 * C6 * satdata->eo * e0dot + sqr(e0dot) + satdata->eo * e0ddot);
	etaddot = (e0ddot + 2.0 * e0dot * tsidottotsi) * s * tsi + eta * tsiddottotsi;
	D18 = tsiddottotsi - sqr(tsidottotsi);
	D19 = -sqr(phidottophi) * (1.0 + 1.0 / sqr(eta)) - eta * etaddot / sqr(phi);
	D1ddot = D1dot * (D14+D15) + D1 * (D18 - 2.0 * D19 + (2.0/3.0) * D17 + 2.0 * sqr(alpha) * sqr(e0dot) / pow(beta, 4.0) + 2.0 * satdata->eo * e0ddot / sqr(beta));
	n0tdot = n0dot * (2.0 * (2.0/3.0) * D17 + 3.0 * (sqr(e0dot) + satdata->eo * e0ddot) / sqr(alpha) - 6.0 * sqr( satdata->eo * e0dot / sqr(alpha)) + 4.0 * D18 - 7.0 * D19 ) + C1dottoC1 * n0ddot;
	n0tdot = n0tdot + C1 * (C1dottoC1 * D16 + D9 * etaddot + D10 * e0ddot + sqr(etadot) * (6.0 + 30.0 * (satdata->eo) * eta + 68.0 * sqr(satdata->eo)) + etadot * e0dot * (40.0 + 30.0 * sqr(eta) + 272.0 * satdata->eo * eta) + sqr(e0dot) * (17.0 + 68.0 * sqr(eta)) + B1 * (D1ddot * D2+ 2.0 * D1dot * D2dot + D1 * (etaddot * D11 + sqr(etadot) * (72. + 54. * sqr(eta))))+ B2 * (D1ddot * D3 + 2.0 * D1dot * D3dot + D1 * (etaddot * D12 + sqr(etadot) * (30.0 + 30.0 * sqr(eta)))) * cos(2.0 * satdata->omegao) + B3 * ((D5dot * D14 + D5 * (D18 - 2.0 * D19)) * D4 + 2.0 * D4dot * D5dot + D5 * (etaddot * D13 + 22.5 * eta * sqr(etadot))) * sin(satdata->omegao) + omega1dot * ((7.0 * (1.0/3.0) * (n0dot / noii) + 4.0 * satdata->eo * e0dot / sqr(beta)) * (C3 * cos(satdata->omegao) - 2.0 * C2 * sin(2.0 * satdata->omegao))+ ((2.0 * C3dot * cos(satdata->omegao) - 4.0 * C2dot * sin(2.0 * satdata->omegao) - omega1dot * (C3 * sin(satdata->omegao) + 4 * C2 * cos(2.0 * satdata->omegao))))));
	smalln0ddot = n0ddot * 1.0E9;
	temp = sqr(smalln0ddot) - n0dot * 1.0E18 * n0tdot;
	p = (temp + sqr(smalln0ddot)) / temp;
	gamma = - n0tdot / (n0ddot * (p - 2));
	nd = n0dot / (p * gamma);
	q = 1.0 - e0ddot / (e0dot * gamma);
	ed = e0dot / (q * gamma);
	/* atmospheric drag and gravitation */
	if ((fabs( n0dot / noii * _xmnpda)) < 0.00216)
        {
		n = noii + n0dot * tsince;
		edot = -(2.0/3.0) * n0dot * (1.0 - satdata->eo) / noii;
		e = satdata->eo + edot * tsince;
		Z1 = 0.5 * n0dot * sqr(tsince);
	} else {
		n = noii + nd * (1.0 - pow((1.0 - gamma * tsince), p));
		edot = e0dot;
		e = satdata->eo + ed * (1.0 - pow((1.0 - gamma * tsince),q));
		Z1 = gamma * tsince;
		Z1 = 1.0 - Z1;
		Z1 = pow(Z1, (p+1.0));
		Z1 = Z1 - 1.0;
		Z1 = Z1 / (gamma * (p+1));
		Z1 = Z1 + tsince;
		Z1 = Z1 * n0dot / (p * gamma);
	}
	omega = satdata->omegao + omegadot * tsince + (7.0 * Z1) / (3.0 * noii) * omega1dot;
	xnode = satdata->xnodeo + xnodedot * tsince + xnode1dot * (7.0 * Z1)/(3.0 * noii);
	M = satdata->xmo + tsince * ldot + Z1 + M1dot * (7.0 * Z1) / (3.0 * noii);
	/* Keple r*/
	M = fmod2p(M);
	temp = M + satdata->eo * sin(M) * (1 + e * cos(M));
	i=1;
	do 
	{
		deltaEw = (M + e * sin(temp) - temp) / (1.0 - e * cos(temp));
		Ew = deltaEw + temp;
		temp = Ew;
		i++;
	} while ((i<=10) && (fabs(deltaEw) > _e6a));
	/* short-period periodics */
	a = pow((xke / n), _tothrd);
	beta = sqrt(1.0 - sqr(e));
	sinf = beta * sin(Ew) * (1.0 / (1.0 - e * cos(Ew)));
	cosf = (cos(Ew) - e) * (1.0 / (1.0 - e * cos(Ew)));
	f = actan(sinf, cosf);
	sinu = sinf * cos(omega) + cosf * sin(omega);
	cosu = cosf * cos(omega) - sinf * sin(omega);
	rii = (a * (1.0 - sqr(e))) / (1.0 + (e * cosf));
	deltaR = (0.5 * CK2 * (1.0 / (a * (1.0 - sqr(e))))) * ((1.0 - sqr(theta)) * (2.0 * sqr(cosu) - 1.0) - 3.0 * (3.0 * sqr(theta) - 1.0)) - (0.25 * A30 * sinio) * sinu;
	temp = 3.0 * (0.5 * CK2 * sqr(1.0 / (a * (1.0 - sqr(e))))) * sinio * (2.0 * sqr(cosu) - 1) - (0.25 * A30 * (1.0 / (a * (1 - sqr(e))))) * e * sin(omega);
	deltaI = temp * cosio;
	deltaU = sin(satdata->xincl / 2) * ((0.5 * CK2 * sqr(1.0 / (a * (1 - sqr(e))))) * (0.5 * (1.0 - 7.0 * sqr(theta)) * (2.0 * sinu * cosu)- 3.0 * (1.0 - 5.0 * sqr(theta)) * (f - M + e * sinf)) - (0.25 * A30 * (1.0 / (a * (1.0 - sqr(e))))) * sinio * cosu * (2.0 + (e*cosf))) - 0.5 * (0.25 * A30 * (1.0 / (a * (1 - sqr(e))))) * sqr(theta) * e * cos(omega) / cos(satdata->xincl/2);
	lambda = f + omega + xnode + (0.5 * CK2 * sqr(1.0 / (a * (1.0 - sqr(e))))) * (0.5 * (1.0 + 6.0 * cosio - 7.0 * sqr(theta)) * (2.0 * sinu * cosu) - 3.0 * ((1.0 - 5.0 * sqr(theta)) + 2.0 * cosio) * (f - M + e * sinf )) + (0.25 * A30 * (1.0 / (a *(1 - sqr(e))))) * sinio * (cosio * e * cos(omega) / (1.0 + cosio) - (2.0 + (e*cosf)) * cosu);
	y4 = sin(satdata->xincl / 2.0) * sinu + cosu * deltaU + 0.5 * sinu * cos(satdata->xincl / 2.0)*deltaI;
	y5 = sin(satdata->xincl / 2.0) * cosu - sinu * deltaU + 0.5 * cosu * cos(satdata->xincl / 2.0)*deltaI;
	r = rii + deltaR;
	rdot = n * a * e * sinf / beta + (-n * sqr(a / rii)) * (1.0 * CK2 * (1.0 / (a * (1.0 - sqr(e)))) * (1.0 - sqr(theta)) * (2.0 * sinu * cosu) + (0.25 * A30 * sinio) * cosu);
	rfdot = n * sqr(a) * beta / rii + (-n * sqr(a/rii)) * deltaR + a * (n * (a / rii)) * sinio * temp;
	/* Unit-orientation vector */
	cosI2 = sqrt(1.0 - sqr(y4) - sqr(y5));
	UV.v[0] = 2.0 * y4 * (y5 * sin(lambda) - y4 * cos(lambda)) + cos(lambda);
	UV.v[1] = -2.0 * y4 * (y5 * cos(lambda) + y4 * sin(lambda)) + sin(lambda);
	UV.v[2] = 2.0 * y4 *cosI2;
	VV.v[0] = 2.0 * y5 * (y5 * sin(lambda) - y4 * cos(lambda)) - sin(lambda);
	VV.v[1] = -2.0 * y5 * (y5 * cos(lambda) + y4 * sin(lambda)) + cos(lambda);
	VV.v[2] = 2.0 * y5 * cosI2;
        /* position + velocity */
	for (i=0;(i<3);i++)
	{
		pos->v[i] = r * UV.v[i];
		vel->v[i] = rdot * UV.v[i] + rfdot * VV.v[i];
        }
}



/* ------------------------------------------------------------------- *
 * ------------------------------- sdp8 ------------------------------ *
 * ------------------------------------------------------------------- */

void sdp8 (double tsince, struct vector *pos, struct vector *vel, struct sgp_data *satdata)
{
	struct val_deep_init val_dpinit;
	struct val_deep_sec val_dpsec;
	struct val_deep_per val_dpper;
	struct vector UV, VV;
	double a1=0, cosi=0, theta2=0, tthmun=0, eosq=0, betao2=0, betao=0, del1=0;
	double a0=0, delo=0, aodp=0, xnodp=0, b=0;
	double po=0, pom2=0, sini=0, sing=0, cosg=0, temp=0, sinio2=0;
	double cosio2=0, theta4=0, unm5th=0, unmth2=0, a3cof=0;
	double pardt1=0, pardt2=0, pardt4=0, xmdt1=0, xgdt1=0, xhdt1=0;
	double xlldot=0, omgdt=0, xnodot=0, tsi=0, eta=0, eta2=0,psim2=0, alpha2=0, eeta=0;
        double z1=0, z7=0, xmamdf=0, omgasm=0, xnodes=0, xn=0, em=0, xmam=0;
	double zc2=0, sine=0, cose=0, zc5=0, cape=0;
        double cos2g=0, d1=0, d2=0, d3=0, d4=0, d5=0, b1=0, b2=0, b3=0, c0=0;
	double c1=0, c4=0, c5=0, xndt=0, xndtn=0, edot=0;
	double am=0, beta2m=0, sinos=0, cosos=0, axnm=0, aynm=0, pm=0;
	double g1=0, g2=0, g3=0, g4=0, g5=0, beta=0, snf=0, csf=0, fm=0;
	double dr=0, diwc=0, di=0, sni2du=0, xlamb=0, y4=0, y5=0, r=0, rdot=0, rvdot=0;
	double snlamb=0, cslamb=0;
        double snfg=0, csfg=0, sn2f2g=0, cs2f2g=0, ecosf=0, g10=0, rm=0, aovr=0, g13=0, g14=0, sini2=0;
	int i;
	double xke=0, s=0, qo=0, qoms2t=0, rho=0, temp7=0, temp2=0, xinc=0;
	rho = (2.461 * 0.00001 * xkmper); 
        /* Constants */
	s = _ae + 78 / xkmper;
	qo = _ae + 120 / xkmper;
	xke = sqrt((3600.0 * ge) / cube(xkmper));
	qoms2t = sqr ( sqr (qo - s));
	temp2 = xke / (satdata->xno);
	a1 = pow (temp2 , _tothrd);
	cosi = cos(satdata->xincl);
	theta2 = cosi * cosi;
	tthmun = 3.0 * theta2 - 1.0;
	eosq = satdata->eo * satdata->eo;
	betao2 = 1.0 - eosq;
	betao = sqrt(betao2);
	del1 = 1.5 * CK2 * tthmun / (a1 * a1 * betao * betao2);
	a0 = a1 * (1.0 - del1 * (0.5 * _tothrd + del1 * (1.0 + 134.0 / 81.0 * del1)));
	delo = 1.5 * CK2 * tthmun / (a0 * a0 * betao * betao2);
	aodp = a0 / (1.0 - delo);
	xnodp = satdata->xno / (1.0 + delo);
	b = 2.0 * (satdata->bstar) / rho;
	po = aodp * betao2;
	pom2 = 1.0 / (po * po);
	sini = sin(satdata->xincl);
	sing = sin(satdata->omegao);
	cosg = cos(satdata->omegao);
	temp = 0.5 * satdata->xincl;
	sinio2 = sin(temp);
	cosio2 = cos(temp);
	theta4 = sqr(theta2);
	unm5th = 1.0 - 5.0 * theta2;
	unmth2 = 1.0 - theta2;
	a3cof =  -XJ3 / CK2 * pow(_ae, 3.0);
	pardt1 = 3.0 * CK2 * pom2 * xnodp;
	pardt2 = pardt1 * CK2 * pom2;
	pardt4 = 1.25 * CK4 * pom2 * pom2 * xnodp;
	xmdt1 = 0.5 * pardt1 * betao * tthmun;
	xgdt1 = -0.5 * pardt1 * unm5th;
	xhdt1 = -pardt1 * cosi;
	xlldot = xnodp + xmdt1 + 0.0625 * pardt2 * betao * (13.0 - 78.0 * theta2 + 137.0 * theta4);
	omgdt = xgdt1 + 0.0625 * pardt2 * (7.0 - 114.0 * theta2 + 395.0 * theta4) + pardt4 * (3.0 - 36.0 * theta2 + 49.0 * theta4);
	xnodot = xhdt1 + (0.5 * pardt2 * (4.0 - 19.0 * theta2) + 2.0 * pardt4 * (3.0 - 7.0 * theta2)) * cosi;
	tsi = 1.0 / ((double) ((double) po - (double) s));
	eta = satdata->eo * s * tsi;
	eta2 = sqr(eta);
	psim2 = fabs(1.0 / (1.0 - eta2));
	alpha2 = 1.0 + eosq;
	eeta = satdata->eo * eta;
	cos2g = 2.0 * sqr(cosg) - 1.0;
	d5 = tsi * psim2;
	d1 = d5 / po;
	d2 = 12.0 + eta2 * (36.0 + 4.5 * eta2);
	d3 = eta2 * (15.0 + 2.5 * eta2);
	d4 = eta * (5.0 + 3.75 * eta2);
	b1 = CK2 * tthmun;
	b2 =  -CK2 * unmth2;
	b3 = a3cof * sini;
	c0 = 0.5 * b * rho * qoms2t * xnodp * aodp * pow(tsi, 4.0) * pow(psim2, 3.5) / sqrt(alpha2);
	c1 = 1.5 * xnodp * sqr(alpha2) * c0;
	c4 = d1 * d3 * b2;
	c5 = d5 * d4 * b3;
	xndt = c1 * ((2.0 + eta2 * (3.0 + 34.0 * eosq) + 5.0 * eeta * (4.0 + eta2) + 8.5 * eosq) + d1 * d2 * b1 +  c4 * cos2g + c5 * sing);
	xndtn = xndt / xnodp;
	edot =  - _tothrd * xndtn * (1.0 - satdata->eo);

	val_dpinit.eosq=eosq;
	val_dpinit.sinio=sini;
	val_dpinit.cosio=cosi;
	val_dpinit.betao=betao;
	val_dpinit.aodp=aodp;
	val_dpinit.theta2=theta2;
	val_dpinit.sing=sing;
	val_dpinit.cosg=cosg;
	val_dpinit.betao2=betao2;
	val_dpinit.xmdot=xlldot;
	val_dpinit.omgdot=omgdt;
	val_dpinit.xnodott=xnodot;
	val_dpinit.xnodpp=xnodp;
	call_dpinit(&val_dpinit, satdata);
	eosq=val_dpinit.eosq;
	sini=val_dpinit.sinio;
	cosi=val_dpinit.cosio;
	betao=val_dpinit.betao;
	aodp=val_dpinit.aodp;
	theta2=val_dpinit.theta2;
	sing=val_dpinit.sing;
	cosg=val_dpinit.cosg;
	betao2=val_dpinit.betao2;
	xlldot=val_dpinit.xmdot;
	omgdt=val_dpinit.omgdot;
	xnodot=val_dpinit.xnodott;
	xnodp=val_dpinit.xnodpp;

	z1 = 0.5 * xndt * tsince * tsince;
	z7 = 3.5 * _tothrd * z1 / xnodp;
	xmamdf = satdata->xmo + xlldot * tsince;
	omgasm = satdata->omegao + omgdt * tsince + z7 * xgdt1;
	xnodes = satdata->xnodeo + xnodot * tsince + z7 * xhdt1;
	xn = xnodp;

	val_dpsec.xmdf=xmamdf;
	val_dpsec.omgadf=omgasm;
	val_dpsec.xnode=xnodes;
	val_dpsec.emm=em;
	val_dpsec.xincc=xinc;
	val_dpsec.xnn=xn;
	val_dpsec.tsince=tsince;
	call_dpsec(&val_dpsec, satdata);
	xmamdf=val_dpsec.xmdf;
	omgasm=val_dpsec.omgadf;
	xnodes=val_dpsec.xnode;
	em=val_dpsec.emm;
	xinc=val_dpsec.xincc;
	xn=val_dpsec.xnn;
	tsince=val_dpsec.tsince;

	xn = xn + xndt * tsince;
	em = em + edot * tsince;
	xmam = xmamdf + z1 + z7 * xmdt1;

	val_dpper.e=em;
	val_dpper.xincc=xinc;
	val_dpper.omgadf=omgasm;
	val_dpper.xnode=xnodes;
	val_dpper.xmam=xmam;
	call_dpper(&val_dpper, satdata);
	em=val_dpper.e;
	xinc=val_dpper.xincc;
	omgasm=val_dpper.omgadf;
	xnodes=val_dpper.xnode;
	xmam=val_dpper.xmam;

	xmam = fmod2p(xmam);
	zc2 = xmam + em * sin(xmam) * (1.0 + em * cos(xmam));
	i = 1;
	do 
	{
		sine = sin(zc2);
		cose = cos(zc2);
		zc5 = 1.0 / (1.0 - em * cose);
		cape = (xmam + em * sine - zc2) * zc5 + zc2;
		temp7 = zc2;
		zc2 = cape;
		i++;
	} while ((i<=10) && (fabs(zc2-temp7) > _e6a));

	am = pow((xke / xn),_tothrd);
	beta2m = 1.0 - em * em;
	sinos = sin(omgasm);
	cosos = cos(omgasm);
	axnm = em * cosos;
	aynm = em * sinos;
	pm = am * beta2m;
	g1 = 1.0 / pm;
	g2 = 0.5 * CK2 * g1;
	g3 = g2 * g1;
	beta = sqrt(beta2m);
	g4 = 0.25 * a3cof * sini;
	g5 = 0.25 * a3cof * g1;
	snf = beta * sine * zc5;
	csf = (cose - em) * zc5;
	fm = actan(snf, csf);
	snfg = snf * cosos + csf * sinos;
	csfg = csf * cosos - snf * sinos;
	sn2f2g = 2.0 * snfg * csfg;
	cs2f2g = 2.0 * sqr(csfg) - 1.0;
	ecosf = em * csf;
	g10 = fm - xmam + em * snf;
	rm = pm / (1.0 + ecosf);
	aovr = am / rm;
	g13 = xn * aovr;
	g14 =  -g13 * aovr;
	dr = g2 * (unmth2 * cs2f2g - 3.0 * tthmun) - g4 * snfg;
	diwc = 3.0 * g3 * sini * cs2f2g - g5 * aynm;
	di = diwc * cosi;
	sini2 = sin(0.5 * xinc);
	sni2du = sinio2 * (g3 * (0.5 * (1.0 - 7.0 * theta2) * sn2f2g - 3.0 * unm5th * g10) - g5 * sini * csfg * (2.0 + ecosf)) - 0.5 * g5 * theta2 * axnm / cosio2;
	xlamb = fm + omgasm + xnodes + g3 * (0.5 * (1.0 + 6.0 * cosi - 7.0 * theta2) * sn2f2g - 3.0 * (unm5th + 2.0 * cosi) * g10) + g5 * sini * (cosi * axnm / (1.0 + cosi) - (2.0 + ecosf) * csfg);
	y4 = sini2 * snfg + csfg * sni2du + 0.5 * snfg * cosio2 * di;
	y5 = sini2 * csfg - snfg * sni2du + 0.5 * csfg * cosio2 * di;
	r = rm + dr;
	rdot = xn * am * em * snf / beta + g14 * (2.0 * g2 * unmth2 * sn2f2g + g4 * csfg);
	rvdot = xn * sqr(am) * beta / rm + g14 * dr + am * g13 * sini * diwc;
	snlamb = sin(xlamb);
	cslamb = cos(xlamb);
	temp = 2.0 * (y5 * snlamb - y4 * cslamb);
	UV.v[0] = y4 * temp + cslamb;
	VV.v[0] = y5 * temp - snlamb;
	temp = 2.0 * (y5 * cslamb + y4 * snlamb);
	UV.v[1] = -y4 * temp + snlamb;
	VV.v[1] = -y5 * temp + cslamb;
	temp = 2.0 * sqrt(1.0 - y4 * y4 - y5 * y5);
	UV.v[2] = y4 * temp;
	VV.v[2] = y5 * temp;

        /* position + velocity */
	for (i=0;(i<3);i++)
	{
		pos->v[i] = r  * UV.v[i];
		vel->v[i] = rdot * UV.v[i] + rvdot * VV.v[i];
        }
}


/* ------------------------------------------------------------------- *
 * ----------------------------- sgp8call ---------------------------- *
 * ------------------------------------------------------------------- */

void sgp8call (double time, struct vector *pos, struct vector *vel, struct sgp_data *satdata)
{
	double tsince;
	tsince = ( time - (satdata->julian_epoch) ) * _xmnpda;
	if ((satdata->ideep)==0)
	    sgp8 (tsince, pos, vel, satdata);
	else
	    sdp8 (tsince, pos, vel, satdata);
}
