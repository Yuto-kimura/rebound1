/**
 * @file 	integrator.c
 * @brief 	Mikkola integration scheme.
 * @author 	Hanno Rein <hanno@hanno-rein.de>
 * @detail	This file implements the Mikkola integration scheme.  
 * 
 * @section 	LICENSE
 * Copyright (c) 2015 Hanno Rein
 *
 * This file is part of rebound.
 *
 * rebound is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * rebound is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with rebound.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <math.h>
#include <time.h>
#include "particle.h"
#include "main.h"
#include "gravity.h"
#include "boundaries.h"

// These variables have no effect for leapfrog.
int integrator_force_is_velocitydependent 	= 1;
double integrator_epsilon 			= 0;
double integrator_min_dt 			= 0;

// Fast inverse factorial lookup table
static const double invfactorial[] = {1., 1., 1./2., 1./6., 1./24., 1./120., 1./720., 1./5040., 1./40320., 1./362880., 1./3628800., 1./39916800., 1./479001600., 1./6227020800., 1./87178291200., 1./1307674368000., 1./20922789888000., 1./355687428096000., 1./6402373705728000., 1./121645100408832000., 1./2432902008176640000., 1./51090942171709440000., 1./1124000727777607680000., 1./25852016738884976640000., 1./620448401733239439360000., 1./15511210043330985984000000., 1./403291461126605635584000000., 1./10888869450418352160768000000., 1./304888344611713860501504000000., 1./8841761993739701954543616000000., 1./265252859812191058636308480000000., 1./8222838654177922817725562880000000., 1./263130836933693530167218012160000000., 1./8683317618811886495518194401280000000., 1./295232799039604140847618609643520000000.};

double c_n_series(unsigned int n, double z){
	double c_n = 0.;
	for (unsigned int j=0;j<13;j++){
		double term = pow(-z,j)*invfactorial[n+2*j];
		c_n += term;
		if (fabs(term/c_n) < 1e-17) break; // Stop if new term smaller than machine precision
	}
	return c_n;
}

double c(unsigned int n, double z){
	if (z>0.5){
		// Speed up conversion with 4-folding formula
		switch(n){
			case 0:
			{
				double cn4 = c(3,z/4.)*(1.+c(1,z/4.))/8.;
				double cn2 = 1./2.-z*cn4;
				double cn0 = 1.-z*cn2;
				return cn0;
			}
			case 1:
			{
				double cn5 = (c(5,z/4.)+c(4,z/4.)+c(3,z/4.)*c(2,z/4.))/16.;
				double cn3 = 1./6.-z*cn5;
				double cn1 = 1.-z*cn3;
				return cn1;
			}
			case 2:
			{
				double cn4 = c(3,z/4.)*(1.+c(1,z/4.))/8.;
				double cn2 = 1./2.-z*cn4;
				return cn2;
			}
			case 3:
			{
				double cn5 = (c(5,z/4.)+c(4,z/4.)+c(3,z/4.)*c(2,z/4.))/16.;
				double cn3 = 1./6.-z*cn5;
				return cn3;
			}
			case 4:
			{
				double cn4 = c(3,z/4.)*(1.+c(1,z/4.))/8.;
				return cn4;
			}
			case 5:
			{
				double cn5 = (c(5,z/4.)+c(4,z/4.)+c(3,z/4.)*c(2,z/4.))/16.;
				return cn5;
			}
		}
	}
	return c_n_series(n,z);
}

double integrator_G(unsigned int n, double beta, double X){
	return pow(X,n)*c(n,beta*X*X);
}

struct particle* p_j  = NULL;
double* eta = NULL;
double* m_j = NULL;

void kepler_step(int i){
	//double M = G*(eta[i]);
	double M = G*(eta[i]/eta[i-1]*particles[0].m);
	struct particle p1 = p_j[i];

	double r0 = sqrt(p1.x*p1.x + p1.y*p1.y + p1.z*p1.z);
	double v2 =  p1.vx*p1.vx + p1.vy*p1.vy + p1.vz*p1.vz;
	double beta = 2.*M/r0 - v2;
	double eta = p1.x*p1.vx + p1.y*p1.vy + p1.z*p1.vz;
	double zeta = M - beta*r0;
	

	double X = 0;  // TODO: find a better initial estimate.
	double G1,G2,G3;
	for (int n_hg=0;n_hg<20;n_hg++){
		G2 = integrator_G(2,beta,X);
		G3 = integrator_G(3,beta,X);
		G1 = X-beta*G3;
		double s   = r0*X + eta*G2 + zeta*G3-dt;
		double sp  = r0 + eta*G1 + zeta*G2;
		double dX  = -s/sp; // Newton's method
		
		//double G0 = 1.-beta*G2;
		//double spp = r0 + eta*G0 + zeta*G1;
		//double dX  = -(s*sp)/(sp*sp-0.5*s*spp); // Householder 2nd order formula
		X+=dX;
		if (fabs(dX/X)<1e-15) break; // TODO: Make sure loop does not get stuck (add maximum number of iterations) 
	}

	double r = r0 + eta*G1 + zeta*G2;
	double f = 1.-M*G2/r0;
	double g = dt - M*G3;
	double fd = -M*G1/(r0*r); 
	double gd = 1.-M*G2/r; 

	p_j[i].x = f*p1.x + g*p1.vx;
	p_j[i].y = f*p1.y + g*p1.vy;
	p_j[i].z = f*p1.z + g*p1.vz;

	p_j[i].vx = fd*p1.x + gd*p1.vx;
	p_j[i].vy = fd*p1.y + gd*p1.vy;
	p_j[i].vz = fd*p1.z + gd*p1.vz;

}


void integrator_part1(){
}


void integrator_to_jacobi(){
	double s_x = particles[0].m * particles[0].x;
	double s_y = particles[0].m * particles[0].y;
	double s_z = particles[0].m * particles[0].z;
	double s_vx = particles[0].m * particles[0].vx;
	double s_vy = particles[0].m * particles[0].vy;
	double s_vz = particles[0].m * particles[0].vz;
	for (int i=1;i<N;i++){
		p_j[i].x = particles[i].x - s_x/eta[i-1];
		p_j[i].y = particles[i].y - s_y/eta[i-1];
		p_j[i].z = particles[i].z - s_z/eta[i-1];
		p_j[i].vx = particles[i].vx - s_vx/eta[i-1];
		p_j[i].vy = particles[i].vy - s_vy/eta[i-1];
		p_j[i].vz = particles[i].vz - s_vz/eta[i-1];
		s_x += particles[i].m * particles[i].x;
		s_y += particles[i].m * particles[i].y;
		s_z += particles[i].m * particles[i].z;
		s_vx += particles[i].m * particles[i].vx;
		s_vy += particles[i].m * particles[i].vy;
		s_vz += particles[i].m * particles[i].vz;
	}
	p_j[0].x = s_x / eta[N-1];
	p_j[0].y = s_y / eta[N-1];
	p_j[0].z = s_z / eta[N-1];
	p_j[0].vx = s_vx / eta[N-1];
	p_j[0].vy = s_vy / eta[N-1];
	p_j[0].vz = s_vz / eta[N-1];
}

void integrator_to_heliocentric(){
	double s_x = 0.;
	double s_y = 0.;
	double s_z = 0.;
	double s_vx = 0.;
	double s_vy = 0.;
	double s_vz = 0.;
	for (int i=N-1;i>0;i--){
		particles[i].x = p_j[0].x + eta[i-1]/eta[i] * p_j[i].x - s_x;
		particles[i].y = p_j[0].y + eta[i-1]/eta[i] * p_j[i].y - s_y;
		particles[i].z = p_j[0].z + eta[i-1]/eta[i] * p_j[i].z - s_z;
		particles[i].vx = p_j[0].vx + eta[i-1]/eta[i] * p_j[i].vx- s_vx;
		particles[i].vy = p_j[0].vy + eta[i-1]/eta[i] * p_j[i].vy- s_vy;
		particles[i].vz = p_j[0].vz + eta[i-1]/eta[i] * p_j[i].vz- s_vz;
		s_x += particles[i].m/eta[i] * p_j[i].x;
		s_y += particles[i].m/eta[i] * p_j[i].y;
		s_z += particles[i].m/eta[i] * p_j[i].z;
		s_vx += particles[i].m/eta[i] * p_j[i].vx;
		s_vy += particles[i].m/eta[i] * p_j[i].vy;
		s_vz += particles[i].m/eta[i] * p_j[i].vz;
	}
	particles[0].x = p_j[0].x - s_x;
	particles[0].y = p_j[0].y - s_y;
	particles[0].z = p_j[0].z - s_z;
	particles[0].vx = p_j[0].vx - s_vx;
	particles[0].vy = p_j[0].vy - s_vy;
	particles[0].vz = p_j[0].vz - s_vz;
}

void integrator_interaction(double _dt){
	double s_x = 0.;
	double s_y = 0.;
	double s_z = 0.;
	for (int i=1;i<N;i++){
		// Eq 132
		double rj3 = pow(p_j[i].x*p_j[i].x + p_j[i].y*p_j[i].y + p_j[i].z*p_j[i].z,3./2.);
		//double M = G*(eta[i]);
		double M = G*(eta[i]/eta[i-1]*particles[0].m);
		double prefac1 = M/rj3;
		p_j[i].ax = prefac1 * p_j[i].x;
		p_j[i].ay = prefac1 * p_j[i].y;
		p_j[i].az = prefac1 * p_j[i].z;
		for (int k=0;k<N;k++){
			if (k!=i){
				double rikx = particles[i].x-particles[k].x;
				double riky = particles[i].y-particles[k].y;
				double rikz = particles[i].z-particles[k].z;
				double rik3 = pow(rikx*rikx + riky*riky + rikz*rikz,3./2.);
				double prefac = G*particles[k].m/rik3;
				p_j[i].ax -= prefac * rikx;
				p_j[i].ay -= prefac * riky;
				p_j[i].az -= prefac * rikz;
			}
			if (k!=i-1){
				double rjkx = particles[i-1].x-particles[k].x;
				double rjky = particles[i-1].y-particles[k].y;
				double rjkz = particles[i-1].z-particles[k].z;
				double rjk3 = pow(rjkx*rjkx + rjky*rjky + rjkz*rjkz,3./2.);
				double prefac = G*particles[i-1].m*particles[k].m/rjk3;
				s_x += prefac * rjkx;
				s_y += prefac * rjky;
				s_z += prefac * rjkz;
			}
		}
		p_j[i].ax += s_x/eta[i-1];
		p_j[i].ay += s_y/eta[i-1];
		p_j[i].az += s_z/eta[i-1];
		p_j[i].vx += _dt * p_j[i].ax;
		p_j[i].vy += _dt * p_j[i].ay;
		p_j[i].vz += _dt * p_j[i].az;
	}
}

void integrator_part2(){
	if (p_j==NULL){
		p_j = malloc(sizeof(struct particle)*N);
		eta = malloc(sizeof(double)*N);
		m_j = malloc(sizeof(double)*N);
		eta[0] = particles[0].m;
		for (int i=1;i<N;i++){
			eta[i] = eta[i-1] + particles[i].m;
		}
		m_j[0] = eta[N-1];
		for (int i=1;i<N;i++){
			m_j[i] = eta[i-1]/eta[i]*particles[i].m;
		}
	}

	integrator_to_jacobi();
	integrator_interaction(dt/2.);

	for (int i=1;i<N;i++){
		kepler_step(i);
	}
	p_j[0].x += dt*p_j[0].vx;
	p_j[0].y += dt*p_j[0].vy;
	p_j[0].z += dt*p_j[0].vz;

	integrator_to_heliocentric();
	integrator_interaction(dt/2.);
	integrator_to_heliocentric();

	t+=dt;
}
	

