/**
 * @file 	tools.h
 * @brief 	Tools for creating distributions.
 * @author 	Hanno Rein <hanno@hanno-rein.de>
 * 
 * @section 	LICENSE
 * Copyright (c) 2011 Hanno Rein, Shangfei Liu
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
#ifndef TOOLS_H
#define TOOLS_H
#include "particle.h"
/**
 * Struct representing a Keplerian orbit.
 */
struct orbit {
	double a;
	double r;	// Radial distance from central object
	double h;	// Angular momentum
	double P;	// Orbital period
	double l;
	double e;
	double inc;
	double Omega; 	// longitude of ascending node
	double omega; 	// argument of perihelion
	double M;  	// mean anomaly
	double E;  	// eccentric anomaly
	double f; 	// true anomaly
};


/**
 * Calculates a random variable in a given range.
 * @param min Minimum value.
 * @param max Maximum value.
 */
double tools_uniform(double min, double max);

/**
 * Calculates a random variable drawn form a powerlaw distribution.
 * @param min Minimum value.
 * @param max Maximum value.
 * @param slop Slope of powerlaw distribution.
 */
double tools_powerlaw(double min, double max, double slope);

/**
 * Calculate a random number with normal distribution.
 * Algorithm by D.E. Knut, 1997, The Art of Computer Programmin, Addison-Wesley. 
 * @param variance Variance of normal distribution.
 * @return Random number with normal distribution (mean 0). 
 */
double tools_normal(double variance);

/**
 * This function calculated orbital elements for a given particle. The center of
 * mass is assumed to be at the origin.
 * @param p Particle.
 * @param cmass Mass of the central object.
 * @return Orbital parameters. 
 */
struct orbit tools_p2orbit(struct particle p, double cmass);


#endif 	// TOOLS_H
