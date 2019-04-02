// Copyright (C) 2017-2019  Waterloo Quantitative Consulting Group, Inc.
// See COPYING and LICENSE files at project root for more details.

/**
 *  @file poisson2d/main.cpp ...
 */

#include "TriangleNodesProvisioner.hpp"
#include "MeshManager.hpp"
#include "VtkOutputter.hpp"
#include "CsvOutputter.hpp"
#include "Types.hpp"
#include "Constants.hpp"
#include "Warning.hpp"
#include "Poisson2d.hpp"
#include "LSERK4.hpp"
#include "GMRESSolver.hpp"
#include <blitz/array.h>
#include <cmath>
#include <string>
#include <stdexcept>

using blitz::firstIndex;
using blitz::secondIndex;
using blitz::thirdIndex;
using blitz::sum;
using blitzdg::normMax;
using blitzdg::poisson2d::PoissonOperator;
using blitzdg::poisson2d::Precon;
using blitzdg::fullToVector;
using blitzdg::constants::pi;
using std::abs;
using std::string;
using std::cout;
using std::endl;

int main(int argc, char **argv) {
	using namespace blitzdg;

	printDisclaimer();

	// Numerical parameters (N = Order of polynomials)
	const index_type N = 1;

	// Build dependencies.
	MeshManager meshManager;
	meshManager.readMesh("input/simple_circle.msh");

	// Dependency-inject mesh manager to nodes provisioner.
	TriangleNodesProvisioner triangleNodesProvisioner(N, meshManager);

#ifndef __MINGW32__
	VtkOutputter outputter(triangleNodesProvisioner);
#else
	CsvOutputter outputter;
#endif

	const DGContext2D& dg = triangleNodesProvisioner.get_DGContext();
	const index_type Np = dg.numLocalPoints(), K = dg.numElements();
	
	real_matrix_type uexact(Np, K), r(Np, K), rhs(Np, K), soln(Np,K);
	const real_matrix_type& x = dg.x(), y = dg.y();

	r = blitz::sqrt(x*x + y*y);
	const real_type r0 = blitz::max(r);

	uexact = sin(pi*(r*r)/(r0*r0));

	rhs = 4*pi/r0/r0*(cos(pi*(r*r)/(r0*r0)) - pi*r*r/(r0*r0)*sin(pi*(r*r)/(r0*r0)) );

	const index_type numGlobalNodes = Np*K;

	const real_matrix_type& Vinv = dg.Vinv();
	real_matrix_type MassMatrix(Np, Np);

	blitz::firstIndex ii;
	blitz::secondIndex jj;
	blitz::thirdIndex kk;

	MassMatrix = blitz::sum(Vinv(kk,ii)*Vinv(kk,jj), kk); // local one.

	real_vector_type MMRHSVec(numGlobalNodes), outVec(numGlobalNodes);

	const bool byRowsOpt = false;

	real_matrix_type MMRHS(Np, K);

	MMRHS = dg.jacobian()*(blitz::sum(MassMatrix(ii,kk)*rhs(kk,jj),kk));
	fullToVector(MMRHS, MMRHSVec, byRowsOpt);

	// Need to initialize to zero to tell gmres we aren't making an initial guess.
	outVec = 0*ii;

	GMRESSolver gmres;
	GMRESParams params;
	params.maxits = 1500;
	params.relTol = 8.e-3;

	GMRESOut result = gmres.solve(PoissonOperator(dg), Precon(), MMRHSVec, outVec, params);

	vectorToFull(outVec, soln, false);

	cout << "result is " << result << "\n";

	std::map<std::string, real_matrix_type> fields;
	fields.insert({"uexact", uexact});
	fields.insert({"rhs", rhs});
	fields.insert({"u", soln});
	outputter.writeFieldsToFiles(fields, 0);

	return 0;
} // end main