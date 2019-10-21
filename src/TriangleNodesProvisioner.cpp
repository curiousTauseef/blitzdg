// Copyright (C) 2017-2019  Waterloo Quantitative Consulting Group, Inc.
// See COPYING and LICENSE files at project root for more details.

#include "TriangleNodesProvisioner.hpp"
#include "DGContext2D.hpp"
#include "CSCMatrix.hpp"
#include "BlitzHelpers.hpp"
#include "DenseMatrixInverter.hpp"
#include "MeshManager.hpp"
#include "Constants.hpp"
#include "Types.hpp"
#include <blitz/array.h>
#include <cmath>
#include <limits>
#include <memory>

using blitz::firstIndex;
using blitz::Range;
using blitz::secondIndex;
using blitz::sum;
using blitz::thirdIndex;
using std::numeric_limits;
using std::unique_ptr;
using std::abs;
using std::sqrt;
using std::log;
using std::vector;
using std::pow;
using std::exp;

namespace blitzdg {
    const index_type TriangleNodesProvisioner::NumFaces = 3;
    const real_type TriangleNodesProvisioner::NodeTol = 1.e-5;
    const real_type pi = blitzdg::constants::pi;

    TriangleNodesProvisioner::TriangleNodesProvisioner(index_type _NOrder, const MeshManager& _MeshManager)
        : NumElements{ _MeshManager.get_NumElements() }, NOrder{ _NOrder },
        NumLocalPoints{ (_NOrder + 2)*(_NOrder+1)/2 },
        NumFacePoints{ _NOrder + 1},
        xGrid{ new real_matrix_type((_NOrder + 2)*(_NOrder+1)/2, _MeshManager.get_NumElements()) },
        yGrid{ new real_matrix_type((_NOrder + 2)*(_NOrder+1)/2, _MeshManager.get_NumElements()) },
        rGrid{ new real_vector_type((_NOrder + 2)*(_NOrder+1)/2) },
        sGrid{ new real_vector_type((_NOrder + 2)*(_NOrder+1)/2) },
        V{ new real_matrix_type((_NOrder + 2)*(_NOrder+1)/2, (_NOrder + 2)*(_NOrder+1)/2) }, 
        Dr{ new real_matrix_type((_NOrder + 2)*(_NOrder+1)/2, (_NOrder + 2)*(_NOrder+1)/2) },
        Ds{ new real_matrix_type((_NOrder + 2)*(_NOrder+1)/2, (_NOrder + 2)*(_NOrder+1)/2) },
        Drw{ new real_matrix_type((_NOrder + 2)*(_NOrder+1)/2, (_NOrder + 2)*(_NOrder+1)/2) },
        Dsw{ new real_matrix_type((_NOrder + 2)*(_NOrder+1)/2, (_NOrder + 2)*(_NOrder+1)/2) },
        Lift{ new real_matrix_type((_NOrder + 2)*(_NOrder+1)/2, (_NOrder+1)*NumFaces) },
        J{ new real_matrix_type((_NOrder + 2)*(_NOrder+1)/2, _MeshManager.get_NumElements()) },
        rx{ new real_matrix_type((_NOrder + 2)*(_NOrder+1)/2, _MeshManager.get_NumElements()) },
        sx{ new real_matrix_type((_NOrder + 2)*(_NOrder+1)/2, _MeshManager.get_NumElements()) },
        ry{ new real_matrix_type((_NOrder + 2)*(_NOrder+1)/2, _MeshManager.get_NumElements()) },
        sy{ new real_matrix_type((_NOrder + 2)*(_NOrder+1)/2, _MeshManager.get_NumElements()) },
        nx{ new real_matrix_type((_NOrder+1)*NumFaces, _MeshManager.get_NumElements()) },
        ny{ new real_matrix_type((_NOrder+1)*NumFaces, _MeshManager.get_NumElements()) },
        Vinv{ new real_matrix_type((_NOrder + 2)*(_NOrder+1)/2, (_NOrder + 2)*(_NOrder+1)/2) },
        Filter{ new real_matrix_type((_NOrder + 2)*(_NOrder+1)/2, (_NOrder + 2)*(_NOrder+1)/2) },
        Fmask{ new index_matrix_type( _NOrder+1, NumFaces) },
        Fscale{ new real_matrix_type((_NOrder+1)*NumFaces, _MeshManager.get_NumElements()) },
        vmapM{ new index_vector_type((_NOrder+1)*NumFaces*_MeshManager.get_NumElements()) },
        vmapP{ new index_vector_type((_NOrder+1)*NumFaces*_MeshManager.get_NumElements()) },
        mapP{ new index_vector_type((_NOrder+1)*NumFaces*_MeshManager.get_NumElements()) },
        BCmap{ new index_hashmap()},
        Mesh2D { _MeshManager },
        Nodes1D{ new Nodes1DProvisioner(_NOrder, 5, -1.0, 1.0) },
		Jacobi{}, Vandermonde{}, LinSolver{}, Inverter{}
    {
        // Nodal construction required for physical simulations.
        buildNodes();
        buildLift();
        buildPhysicalGrid();
        buildMaps();
    }

    bool TriangleNodesProvisioner::distanceLessThanEps(real_type x1, real_type y1, real_type x2, real_type y2, real_type eps) {
        if (hypot(x2-x1, y2-y1) < eps) 
            return true;

        return false;
    }


	void TriangleNodesProvisioner::evaluateSimplexPolynomial(const real_vector_type & a, const real_vector_type & b, index_type i, index_type j, real_vector_type & p) const {
		real_vector_type h1(a.length(0));
		real_vector_type h2(b.length(0));

		Jacobi.computeJacobiPolynomial(a, 0.0, 0.0, i, h1);
		Jacobi.computeJacobiPolynomial(b, 2.0*i+1.0, 0.0, j, h2);

		real_vector_type c(b.length(0));

		p = sqrt(2.0)*h1*h2*pow(1.-b, i);
	}

    void TriangleNodesProvisioner::rsToab(const real_vector_type & r, const real_vector_type & s, real_vector_type & a, real_vector_type & b) const {
        index_type Np = r.length(0);
        for( index_type i=0; i < Np; i++) {
            if(s(i) != 1.0) 
                a(i) = 2.0*(1.0+r(i))/(1.0-s(i)) - 1.0;
            else
                a(i) = -1.0;
        }
        b = s;
    }

    void TriangleNodesProvisioner::xyTors(const real_vector_type & x, const real_vector_type & y, real_vector_type & r, real_vector_type & s) const {
        const index_type Np = x.length(0);
        real_vector_type L1(Np), L2(Np), L3(Np);
        L1 = (sqrt(3.0)*y+1.0)/3.0;
        L2 = (-3.0*x - sqrt(3.0)*y + 2.0)/6.0;
        L3 = ( 3.0*x - sqrt(3.0)*y + 2.0)/6.0;

        r =-L2 + L3 - L1;
        s =-L2 - L3 + L1;
    }


    void TriangleNodesProvisioner::computeVandermondeMatrix(index_type N, const real_vector_type & r, const real_vector_type & s, real_matrix_type & V) const {
        const index_type Nr = r.length(0);

        real_vector_type a(Nr), b(Nr);
        rsToab(r, s, a, b);

        // build the Vandermonde matrix
        index_type count = 0;
        for (index_type i=0; i < N + 1; ++i) {
            for (index_type j=0; j < N - i + 1; ++j) {
                real_vector_type p(Nr);
                evaluateSimplexPolynomial(a,b,i,j,p);
                V(Range::all(),count) = p;
                ++count;
            }
        }
        real_matrix_type& VinvRef = *Vinv;
        Inverter.computeInverse(V, VinvRef);
    }

    void TriangleNodesProvisioner::computeGradVandermondeMatrix(index_type N,  const real_vector_type & r, const real_vector_type & s, real_matrix_type & V2Dr, real_matrix_type & V2Ds) const {
        const index_type Nr = r.length(0);
        const index_type Np = (N+1)*(N+2)/2;

        real_vector_type a(Nr), b(Nr);

        rsToab(r, s, a, b);

        index_type count = 0;
        for (index_type i=0; i <= N; ++i) {
            for (index_type j=0; j <= N-i; ++j) {

                real_vector_type v2drCol(Np), v2dsCol(Np);
                evaluateGradSimplex(a, b, i, j, v2drCol, v2dsCol);

                V2Dr(Range::all(), count) = v2drCol;
                V2Ds(Range::all(), count) = v2dsCol;
                ++count;
            }
        }
    }

    void TriangleNodesProvisioner::computeDifferentiationMatrices(const real_matrix_type & V2Dr, const real_matrix_type & V2Ds, const real_matrix_type & V, real_matrix_type & Dr, real_matrix_type & Ds, real_matrix_type& Drw, real_matrix_type& Dsw) const {
        firstIndex ii;
        secondIndex jj;
        thirdIndex kk;
 
        const index_type numRowsV = V.rows();
        const index_type numColsV = V.cols();


		// Note: this is not a column major ordering trick. We need these transposes.
        real_matrix_type Vtrans(numColsV, numRowsV);
        real_matrix_type V2Drtrans(numColsV, numRowsV);
        real_matrix_type V2Dstrans(numColsV, numRowsV);

        real_matrix_type Drtrans(numColsV, numRowsV);
        real_matrix_type Dstrans(numColsV, numRowsV);

        Drtrans = 0.*jj;
        Dstrans = 0.*jj;

        Vtrans = V(jj, ii);
        V2Drtrans = V2Dr(jj,ii);
        V2Dstrans = V2Ds(jj,ii);

        // Dr = V2Dr * V^{-1}
        LinSolver.solve(Vtrans, V2Drtrans, Drtrans);

        // LAPACK can mangle our input.
        Vtrans = V(jj,ii);

        // Ds = V2Ds / V;
        LinSolver.solve(Vtrans, V2Dstrans, Dstrans);

        // Take transpose.
        Dr = Drtrans(jj, ii); 
        Ds = Dstrans(jj, ii);

        // Now construct the weak derivatives.
        real_matrix_type VVt(numRowsV,numRowsV), VVrt(numRowsV, numRowsV), VVst(numRowsV, numRowsV); 
        real_matrix_type VVttrans(numRowsV,numRowsV), VVrttrans(numRowsV, numRowsV), VVsttrans(numRowsV, numRowsV); 
        real_matrix_type Drwtrans(numRowsV,numRowsV), Dswtrans(numRowsV,numRowsV);
        VVt = blitz::sum(V(ii,kk)*Vtrans(kk,jj), kk);
        VVrt = blitz::sum(V(ii,kk)*V2Drtrans(kk,jj), kk);
        VVst = blitz::sum(V(ii,kk)*V2Dstrans(kk,jj), kk);

        VVttrans = VVt(jj,ii);
        VVrttrans = VVrt(jj,ii);
        VVsttrans = VVst(jj,ii);

        LinSolver.solve( VVttrans, VVrttrans,  Drwtrans );
        LinSolver.solve( VVttrans, VVsttrans,  Dswtrans );

        Drw = Drwtrans(jj,ii);
        Dsw = Dswtrans(jj,ii);
    }

    void TriangleNodesProvisioner::buildFilter(real_type Nc, index_type s) {
        firstIndex ii;
        secondIndex jj;
        thirdIndex kk;

        real_type alpha = -std::log(std::numeric_limits<real_type>().epsilon());

        real_matrix_type& F = *Filter;
        real_matrix_type& Vref = *V;
        real_matrix_type& Vinvref = *Vinv;

        real_matrix_type Fdiag(NumLocalPoints, NumLocalPoints);
        Fdiag = 0.0*jj;

        // build exponential filter
        index_type count = 0;
        for (index_type i=0; i <= NOrder; ++i) {
            for (index_type j=0; j <= NOrder-i; ++j) {
                if ( (i+j) >= Nc) {
                    real_type k = (static_cast<real_type>(i+j) - Nc) / (static_cast<real_type>(NOrder) - Nc);
                    Fdiag(count, count) = std::exp(-alpha*std::pow(k,s));
                } else {
                    Fdiag(count, count) = 1.0;
                }
                ++count;
            }
        }
        
        real_matrix_type tmp(NumLocalPoints, NumLocalPoints);
        tmp = sum(Fdiag(ii,kk)*Vinvref(kk,jj), kk);

        F = sum(Vref(ii,kk)*tmp(kk,jj), kk);
    }

    void TriangleNodesProvisioner::computeEquilateralNodes(real_vector_type & x, real_vector_type & y) const {
        real_vector_type alphaOptimal(15);

        alphaOptimal = 0.0000,0.0000,1.4152,0.1001,0.2751,0.9800,1.0999,
                       1.2832,1.3648,1.4773,1.4959,1.5743, 1.5770,1.6223,1.6258;

        real_type alpha = 2.0/3.0;
        if (NOrder < 16) {
            alpha = alphaOptimal(NOrder-1);
        }

        const index_type Np = (NOrder+1)*(NOrder+2)/2;

        // Create equidistributed nodes on equilateral triangle.
        real_vector_type L1(Np), L2(Np), L3(Np);

        index_type count = 0;

        for (index_type n=1; n <= NOrder+1; ++n) {
            for (index_type m=1; m <= NOrder+2-n; ++m) {
                L1(count) = (n-1.0)/NOrder;
                L3(count) = (m-1.0)/NOrder;
                ++count;
            }
        }
        L2 = 1.0 - L1 - L3;

        x = -L2+L3; 
        y = (-L2-L3+2*L1)/sqrt(3.0);

        // Compute blending function at each node for each edge.
        real_vector_type blend1(Np), blend2(Np), blend3(Np);
        blend1 = 4*L2*L3; 
        blend2 = 4*L1*L3; 
        blend3 = 4*L1*L2;

        // Amount of warp for each node, for each edge.
        real_vector_type warpf1(Np), warpf2(Np), warpf3(Np), temp(Np);

        temp = L3 - L2; computeWarpFactor(temp, warpf1);
        temp = L1 - L3; computeWarpFactor(temp, warpf2);
        temp = L2 - L1; computeWarpFactor(temp, warpf3);

        // Combine blend & warp
        real_vector_type warp1(Np), warp2(Np), warp3(Np);
		const real_type alphaSquared = alpha*alpha;
        warp1 = blend1*warpf1*(1 + alphaSquared*L1*L1);
        warp2 = blend2*warpf2*(1 + alphaSquared*L2*L2);
        warp3 = blend3*warpf3*(1 + alphaSquared*L3*L3);

        // Accumulate deformations associated with each edge.
        x += + 1*warp1 + cos(2*pi/3)*warp2 + cos(4*pi/3)*warp3;
        y += + 0*warp1 + sin(2*pi/3)*warp2 + sin(4*pi/3)*warp3;
    }

    void TriangleNodesProvisioner::computeWarpFactor(const real_vector_type & r, real_vector_type & warpFactor) const {
        firstIndex ii;
        secondIndex jj;

        const index_type Np = NOrder+1;
        const index_type Nr = r.length(0);

        real_vector_type req(Np), rLGL(Np);
        req = -1.0 + 2*ii/(Np-1.0);

        Jacobi.computeGaussLobottoPoints(0.0, 0.0, NOrder, rLGL);

        real_matrix_type Veq(Np,Np), Veqinv(Np,Np);
        Vandermonde.computeVandermondeMatrix(req, Veq, Veqinv);

        // Evaluate Lagrange polynomial at r.
        real_matrix_type P(Np, Nr), L(Np,Nr);
        for (index_type i=0; i< Np; i++) {
            real_vector_type p(Nr);
            Jacobi.computeJacobiPolynomial(r, 0.0, 0.0, i, p);
            P(i, Range::all()) = p;
        }

        real_matrix_type VeqT(Np,Np);
        VeqT = Veq(jj,ii);

        LinSolver.solve(VeqT, P,  L);

        // warp = L^T * (rLGL - req)
        warpFactor = sum(L(jj,ii)*(rLGL(jj) - req(jj)), jj);

        // Scaling.
        real_vector_type zf(Nr), sf(Nr);
        zf = (abs(r) < 1.0-1e-10);
        sf = 1.0 - (zf*r)*(zf*r); 
        warpFactor = warpFactor / sf + warpFactor*(zf-1.0);
    }

    void TriangleNodesProvisioner::evaluateGradSimplex(const real_vector_type & a, const real_vector_type & b, index_type id, index_type jd, real_vector_type & dpdr, real_vector_type & dpds) const {
        const index_type Np = a.length(0);

        real_vector_type fa(Np), gb(Np), dfa(Np), dgb(Np), tmp(Np);

        Jacobi.computeJacobiPolynomial(a, 0.,       0., id, fa);
        Jacobi.computeJacobiPolynomial(b, 2.*id+1., 0., jd, gb);

        Jacobi.computeGradJacobi(a,       0., 0., id, dfa);
        Jacobi.computeGradJacobi(b, 2.*id+1., 0., jd, dgb);

        // r-derivative
        // d/dr = da/dr d/da + db/dr d/db = (2/(1-s)) d/da = (2/(1-b)) d/da
        dpdr = dfa*gb;
        if (id > 1)
            dpdr *= pow(0.5*(1.-b), id-1);

        // s-derivative
        // d/ds = ((1+a)/2)/((1-b)/2) d/da + d/db
        dpds = dfa*(gb*(0.5*(1+a)));
        if ( id > 1 )
            dpds *= pow(0.5*(1.-b), id-1);

		tmp = dgb*pow(0.5*(1.-b), id);
        if( id > 0 ) {
			tmp -= 0.5*id*gb*pow(0.5*(1-b), id-1);
		}
			

        dpds += fa*tmp;

        // Normalize
        dpdr = pow(2., id+0.5)*dpdr; 
        dpds = pow(2., id+0.5)*dpds;
    }

    void TriangleNodesProvisioner::buildNodes() {
        firstIndex ii;
        secondIndex jj;

        real_vector_type x(NumLocalPoints), y(NumLocalPoints);

        real_vector_type& r = *rGrid.get();
        real_vector_type& s = *sGrid.get();

        computeEquilateralNodes(x, y);
        xyTors(x, y, r, s);

        real_vector_type fmask1(NumFacePoints), fmask2(NumFacePoints), fmask3(NumFacePoints), testField(NumLocalPoints);

        testField = s+1;
        index_type count = 0;
        fmask1 = 0*ii;
        for (index_type i=0; i < NumLocalPoints; i++) {
            if (abs(testField(i)) < NodeTol) {
                fmask1(count) = i;
                ++count;
            }
        }

        testField = r+s;
        count = 0;
        fmask2 = 0*ii;
        for (index_type i=0; i < NumLocalPoints; i++) {
            if (abs(testField(i)) < NodeTol) {
                fmask2(count) = i;
                ++count;
            }
        }

        testField = r+1;
        count = 0;
        fmask3 = 0*ii;
        for (index_type i=0; i < NumLocalPoints; i++) {
            if (abs(testField(i)) < NodeTol) {
                fmask3(count) = i;
                ++count;
            }
        }

        // TODO: Move Fmask computation to a helper method.
        index_matrix_type Fm = *Fmask.get();
        Fm = 0*jj;
        Fm(Range::all(), 0) = fmask1;
        Fm(Range::all(), 1) = fmask2;
        Fm(Range::all(), 2) = fmask3;
    }

    void TriangleNodesProvisioner::buildPhysicalGrid() {
        firstIndex ii;
        secondIndex jj;
        thirdIndex kk;

        // Get element data
        const index_vector_type& EToV = Mesh2D.get_Elements();
        const real_vector_type&  Vert = Mesh2D.get_Vertices();
        index_type NumVertices = Mesh2D.get_NumVerts();

        NumElements = Mesh2D.get_NumElements();

        real_vector_type VX(NumVertices), VY(NumVertices), VZ(NumVertices);
        index_vector_type va(NumElements), vb(NumElements), vc(NumElements);
        index_type count=0;

        // Unpack 1D arrays storing EToV and Vertex coordinates
        for (index_type i=0; i < NumElements; ++i) {
            va(i) = EToV(count);
            vb(i) = EToV(count+1);
            vc(i) = EToV(count+2);
            count += 3;
        }

        count=0;
        for (index_type i=0; i < NumVertices; ++i) {
            VX(i) = Vert(count);
            VY(i) = Vert(count+1);
            VZ(i) = Vert(count+2);
            count += 3;
        }

        real_vector_type VXa(NumElements), VXb(NumElements), VXc(NumElements);
        real_vector_type VYa(NumElements), VYb(NumElements), VYc(NumElements);
        for (index_type i=0; i < NumElements; ++i) {
            VXa(i) = VX(va(i));
            VXb(i) = VX(vb(i));
            VXc(i) = VX(vc(i));

            VYa(i) = VY(va(i));
            VYb(i) = VY(vb(i));
            VYc(i) = VY(vc(i));
        }

        const real_vector_type& r = *rGrid;
        const real_vector_type& s = *sGrid;
        real_matrix_type& V2D = *V;

        index_type Np = NumLocalPoints;
        index_type K = NumElements;

        real_matrix_type V2Dr(Np,Np), V2Ds(Np,Np);
        
        real_matrix_type& D_r = *Dr;
        real_matrix_type& D_s = *Ds;

        real_matrix_type& D_rw = *Drw;
        real_matrix_type& D_sw = *Dsw;

        computeVandermondeMatrix(NOrder, r, s, V2D);
        computeGradVandermondeMatrix(NOrder, r, s, V2Dr, V2Ds);
        computeDifferentiationMatrices(V2Dr, V2Ds, V2D, D_r, D_s, D_rw, D_sw);

        real_matrix_type& x = *xGrid;
        real_matrix_type& y = *yGrid;

        real_matrix_type& Jac = *J;

        real_matrix_type& r_x = *rx;
        real_matrix_type& r_y = *ry;
        real_matrix_type& s_x = *sx;
        real_matrix_type& s_y = *sy;

        real_matrix_type xr(Np,K), yr(Np,K), xs(Np,K), ys(Np,K);

        x = 0.5*(-(r(ii)+s(ii) + 0.*jj)*VXa(jj) + (1+r(ii) + 0.*jj)*VXb(jj) + (1+s(ii) + 0.*jj)*VXc(jj));
        y = 0.5*(-(r(ii)+s(ii) + 0.*jj)*VYa(jj) + (1+r(ii) + 0.*jj)*VYb(jj) + (1+s(ii) + 0.*jj)*VYc(jj));

        xr = sum(D_r(ii,kk)*x(kk,jj), kk);
        yr = sum(D_r(ii,kk)*y(kk,jj), kk);
        xs = sum(D_s(ii,kk)*x(kk,jj), kk);
        ys = sum(D_s(ii,kk)*y(kk,jj), kk);

        Jac = xr*ys - xs*yr;

        // Invert the 2x2 mapping matrix.
        r_x = ys/Jac;
        r_y =-xs/Jac;
        s_x =-yr/Jac;
        s_y = xr/Jac;

        // Normals and Face scalings:

        const index_matrix_type& Fmsk = *Fmask;
        real_matrix_type& Fscal = *Fscale;

        // interpolate geometric factors to face nodes
        index_type numLocalFaceNodes = NumFaces*NumFacePoints;

        real_matrix_type fxr(numLocalFaceNodes, NumElements), fxs(numLocalFaceNodes, NumElements);
        real_matrix_type fyr(numLocalFaceNodes, NumElements), fys(numLocalFaceNodes, NumElements);

        for (index_type k=0; k < NumElements; ++k) {
            index_type count=0;
            for (index_type f=0; f < NumFaces; ++f) {
                for (index_type n=0; n < NumFacePoints; ++n) {
                    fxr(count, k) = xr(Fmsk(n, f), k);
                    fxs(count, k) = xs(Fmsk(n, f), k);
                    fyr(count, k) = yr(Fmsk(n, f), k);
                    fys(count, k) = ys(Fmsk(n, f), k);
                    ++count;
                }
            }
        }

        // build normals.
        real_matrix_type& n_x = *nx, n_y = *ny;

        real_matrix_type norm(numLocalFaceNodes, NumElements, ColumnMajorOrder());

        index_vector_type fid1(NumFacePoints), fid2(NumFacePoints), fid3(NumFacePoints);

        fid1 = ii;
        fid2 = ii + NumFacePoints;
        fid3 = ii + 2*NumFacePoints;

        for(index_type k=0; k < NumElements; ++k) {
            for (index_type i=0; i < NumFacePoints; i++) {
                // face 1
                n_x(fid1(i), k) =  fyr(fid1(i), k);
                n_y(fid1(i), k) = -fxr(fid1(i), k);

                // face 2
                n_x(fid2(i), k) =  fys(fid2(i), k)-fyr(fid2(i), k);
                n_y(fid2(i), k) = -fxs(fid2(i), k)+fxr(fid2(i), k);

                // face 3
                n_x(fid3(i), k) = -fys(fid3(i), k);
                n_y(fid3(i), k) =  fxs(fid3(i), k);
            }
        }

        // normalise
        norm = sqrt(n_x*n_x + n_y*n_y);

        n_x = n_x/norm;
        n_y = n_y/norm;

        for(index_type k=0; k < NumElements; ++k) {
            index_type count=0;
            for (index_type f=0; f < NumFaces; ++f) {
                for (index_type n=0; n < NumFacePoints; n++) {
                    Fscal(count,k) = norm(count,k)/(Jac(Fmsk(n,f),k));
                    ++count;
                }
            }
        }
    }
    
    void TriangleNodesProvisioner::buildMaps() {
        firstIndex ii;
        secondIndex jj;
        thirdIndex kk;

        index_matrix_type nodeIds(NumLocalPoints, NumElements);
        
        const index_matrix_type& Fmsk = *Fmask;
        index_vector_type& vmM = *vmapM;
        index_vector_type& vmP = *vmapP;
        index_vector_type& mP = *mapP;

        const index_vector_type& E2E = Mesh2D.get_EToE();
        const index_vector_type& E2F = Mesh2D.get_EToF();
        const index_vector_type& E2V = Mesh2D.get_Elements(); 

        real_matrix_type xmat(NumLocalPoints, NumElements, ColumnMajorOrder());
        real_matrix_type ymat(NumLocalPoints, NumElements, ColumnMajorOrder());

        xmat = *xGrid; ymat = *yGrid;

        real_vector_type x(NumElements*NumLocalPoints);
        real_vector_type y(NumElements*NumLocalPoints);
        reshapeMatTo1D(xmat, x.data(), false);
        reshapeMatTo1D(ymat, y.data(), false);

        // Assemble global volume node numbering.
        nodeIds = ii + NumLocalPoints*jj;

        index_tensor3_type vmapM3(NumFacePoints, NumFaces, NumElements);
        index_tensor3_type vmapP3(NumFacePoints, NumFaces, NumElements);
        index_tensor3_type mapP3(NumFacePoints, NumFaces, NumElements);

        vmapM3 = 0*kk; vmapP3 = 0*kk; mapP3 = 0*kk;

        for (index_type k=0; k < NumElements; ++k) {
            for (index_type f=0; f < NumFaces; ++f) {
                for (index_type n=0; n < NumFacePoints; ++n) {
                    vmapM3(n,f,k) = nodeIds(Fmsk(n,f), k);
                }
            }
        }

        // Find index of face nodes with respect to volume node ordering.
        for (index_type n=0; n < NumFacePoints; ++n) {
            for (index_type f=0; f < NumFaces; ++f) {
                for (index_type k=0; k < NumElements; ++k) {
                    // find neighbor.
                    index_type k2 = E2E(NumFaces*k + f);
                    index_type f2 = E2F(NumFaces*k + f);
                    const real_vector_type Vert = Mesh2D.get_Vertices();                    

                    index_type v1 = E2V(k*NumFaces + f); 
                    index_type v2 = E2V(k*NumFaces + ((f+1) % NumFaces));

                    // Compute reference length of edge.
                    real_type vx1 = Vert(3*v1), vy1 = Vert(3*v1 + 1);
                    real_type vx2 = Vert(3*v2), vy2 = Vert(3*v2 + 1);

                    real_type refd = hypot(vx1-vx2, vy1-vy2);

                    // find find volume node numbers of left and right nodes 
                    index_type vidM = vmapM3(n,f,k);
                    real_type x1 = x(vidM), y1 = y(vidM);
                    
                    for (index_type nP=0; nP < NumFacePoints; ++nP) {
                        index_type vidP = vmapM3(nP,f2,k2);
                        real_type x2 = x(vidP), y2 = y(vidP);

                        if (distanceLessThanEps(x1,y1,x2,y2, refd*NodeTol)) {
                            vmapP3(n,f,k) = vidP;
                            mapP3(n,f,k) = nP + f2*NumFacePoints+k2*NumFaces*NumFacePoints;
                        }
                    }
                }
            }
        }

        // reshape to 1D.
        index_type count = 0;
        for (index_type k=0; k < NumElements; ++k) {
            for (index_type f=0; f < NumFaces; ++f) {
                for (index_type n=0; n < NumFacePoints; ++n) {
                    vmM(count) = vmapM3(n,f,k);
                    vmP(count) = vmapP3(n,f,k);
                    mP(count) = mapP3(n,f,k);
                    ++count;
                }
            }
        }


        // identify all boundary nodes
        index_vector_type tmpMapB(NumElements*NumFaces*NumFacePoints);
        index_type numBoundaryNodes = 0;
        for (index_type i=0; i< NumElements*NumFaces*NumFacePoints; ++i) {
            if (vmP(i) == vmM(i)) {
                tmpMapB(numBoundaryNodes) = i;
                ++numBoundaryNodes;
            }
        }
        
        mapB = unique_ptr<index_vector_type>(new index_vector_type(numBoundaryNodes));
        vmapB = unique_ptr<index_vector_type>(new index_vector_type(numBoundaryNodes));

        index_vector_type& mB = *mapB;
        index_vector_type& vmB = *vmapB;
        for (index_type i=0; i < numBoundaryNodes; ++i)  {
            mB(i) = tmpMapB(i);
            vmB(i) = vmM(mB(i));
        }

        buildBCHash();
    }

    void TriangleNodesProvisioner::buildBCHash() {
        const index_vector_type& bcVec = Mesh2D.get_BCType();

        buildBCHash(bcVec);
    }

    void TriangleNodesProvisioner::buildBCHash(const index_vector_type& bcType) {
        firstIndex ii;
        secondIndex jj;
    
        index_hashmap& bcMap = *BCmap;

        // create boundary face nodes global numbering.
        index_matrix_type boundaryNodesMat(NumFacePoints, NumFaces*NumElements, ColumnMajorOrder());

        index_vector_type ones(NumFacePoints);
        ones = 0*ii + 1;
        boundaryNodesMat = ones(ii)*bcType(jj);

        index_vector_type boundaryNodes(NumFacePoints*NumFaces*NumElements);
        fullToVector(boundaryNodesMat, boundaryNodes, false);

        index_type count=0;
        for (auto itr = boundaryNodes.begin(); itr != boundaryNodes.end(); ++itr) {
            index_type bct = *itr;

            if (bct != 0) {
                auto search = bcMap.find(bct);
                if (search == bcMap.end())
                    bcMap.insert({ bct, vector<index_type>{ count } });
                else
                    search->second.push_back(count);
            }
            ++count;
        }
    }


    void TriangleNodesProvisioner::buildLift() {
        firstIndex ii;
        secondIndex jj;
        thirdIndex kk;

        real_matrix_type E(NumLocalPoints, NumFaces*NumFacePoints), MassInv(NumLocalPoints, NumLocalPoints);

        real_matrix_type & Liftref = *Lift.get();

        real_vector_type& r = *rGrid.get();
        real_vector_type& s = *sGrid.get();

        index_matrix_type Fm = *Fmask.get();


        real_vector_type faceR(NumFacePoints), faceS(NumFacePoints);
        real_matrix_type V1D(NumFacePoints, NumFacePoints), V1Dinv(NumFacePoints,NumFacePoints);
        real_matrix_type massEdgeInv(NumFacePoints, NumFacePoints);

        real_matrix_type massEdge1(NumFacePoints, NumFacePoints), massEdge2(NumFacePoints,NumFacePoints),
                         massEdge3(NumFacePoints, NumFacePoints);

        // Face 1
        for (index_type i=0; i < NumFacePoints; ++i)
            faceR(i) = r(Fm(i, 0));

        Vandermonde.computeVandermondeMatrix(faceR, V1D, V1Dinv);
        massEdgeInv = sum(V1D(ii,kk)*V1D(jj,kk), kk);

        Inverter.computeInverse(massEdgeInv, massEdge1);

        E = 0.0*jj;
        for (index_type i=0; i < NumFacePoints; ++i) {
            for (index_type j=0; j < NumFacePoints; ++j) {
                E(Fm(i,0),j) = massEdge1(i,j);
            }
        }

        // Face 2
        for (index_type i=0; i < NumFacePoints; ++i)
            faceR(i) = r(Fm(i, 1));

        Vandermonde.computeVandermondeMatrix(faceR, V1D, V1Dinv);
        massEdgeInv = sum(V1D(ii,kk)*V1D(jj,kk), kk);

        Inverter.computeInverse(massEdgeInv, massEdge2);
        for (index_type i=0; i < NumFacePoints; ++i) {
            for (index_type j=NumFacePoints; j < 2*NumFacePoints; ++j) {
                E(Fm(i,1),j) = massEdge2(i,j - NumFacePoints);
            }
        }

        // Face 3.
        for (index_type i=0; i < NumFacePoints; ++i)
            faceS(i) = s(Fm(i, 2));

        Vandermonde.computeVandermondeMatrix(faceS, V1D, V1Dinv);
        massEdgeInv = sum(V1D(ii,kk)*V1D(jj,kk), kk);

        Inverter.computeInverse(massEdgeInv, massEdge3);
        for (index_type i=0; i < NumFacePoints; ++i) {
            for (index_type j=2*NumFacePoints; j < 3*NumFacePoints; ++j) {
                E(Fm(i,2),j) = massEdge3(i,j - 2*NumFacePoints);
            }
        }

        // Compute the Vandermonde guy.
        real_matrix_type& V2D = *V;
        V2D = 0.0*jj;
        computeVandermondeMatrix(NOrder, r, s, V2D);

        MassInv = 0.0*jj;
        MassInv = sum(V2D(ii,kk)*V2D(jj,kk), kk);

        // Multiply by inverse mass matrix;
        Liftref = sum(MassInv(ii,kk)*E(kk,jj),kk);
    }

    void TriangleNodesProvisioner::computeInterpMatrix(const real_vector_type& rout, const real_vector_type& sout, real_matrix_type& IM) const {
        firstIndex ii;
        secondIndex jj;
        thirdIndex kk;

        index_type length = rout.size();

        real_matrix_type Vout(length, NumLocalPoints);
        computeVandermondeMatrix(NOrder, rout, sout, Vout);

        real_matrix_type& invV = *Vinv;
        IM = sum(Vout(ii,kk)*invV(kk,jj),kk);
    }

    const real_matrix_type & TriangleNodesProvisioner::get_Lift() const {
        return *Lift;
    }

    const real_matrix_type & TriangleNodesProvisioner::get_Dr() const {
        return *Dr;
    }

    const real_matrix_type & TriangleNodesProvisioner::get_Ds() const {
        return *Ds;
    }

    const real_matrix_type & TriangleNodesProvisioner::get_Drw() const {
        return *Drw;
    }

    const real_matrix_type & TriangleNodesProvisioner::get_Dsw() const {
        return *Dsw;
    }

    const real_matrix_type & TriangleNodesProvisioner::get_rx() const {
        return *rx;
    }

    const real_matrix_type & TriangleNodesProvisioner::get_ry() const {
        return *ry;
    }

    const real_matrix_type & TriangleNodesProvisioner::get_sx() const {
        return *sx;
    }

    const real_matrix_type & TriangleNodesProvisioner::get_sy() const {
        return *sy;
    }

    const real_matrix_type & TriangleNodesProvisioner::get_xGrid() const {
        return *xGrid;
    }

    const real_matrix_type & TriangleNodesProvisioner::get_yGrid() const {
        return *yGrid;
    }

    index_type TriangleNodesProvisioner::get_NumLocalPoints() const {
        return NumLocalPoints;
    }

    index_type TriangleNodesProvisioner::get_NumFacePoints() const {
        return NumFacePoints;
    }

    const index_vector_type& TriangleNodesProvisioner::get_vmapM() const {
        return *vmapM;
    }

    const index_vector_type& TriangleNodesProvisioner::get_vmapP() const {
        return *vmapP;
    }

    const index_vector_type& TriangleNodesProvisioner::get_vmapB() const {
        return *vmapB;
    }

    const index_vector_type& TriangleNodesProvisioner::get_mapB() const {
        return *mapB;
    }
    const real_matrix_type& TriangleNodesProvisioner::get_Filter() const{
        return *Filter;
    }
    const index_hashmap& TriangleNodesProvisioner::get_bcMap() const {
        return *BCmap;
    }

    const real_matrix_type& TriangleNodesProvisioner::get_Fscale() const {
        return *Fscale;
    }

    const real_matrix_type& TriangleNodesProvisioner::get_nx() const {
        return *nx;
    }

    const real_matrix_type& TriangleNodesProvisioner::get_ny() const {
        return *ny;
    }

    int TriangleNodesProvisioner::get_NumElements() const {
        return NumElements;
    }

    DGContext2D TriangleNodesProvisioner::get_DGContext() const {
        return DGContext2D {
            NOrder,
            NumLocalPoints,
            NumFacePoints,
            NumElements,
            NumFaces,
            Filter.get(),
            rGrid.get(),
            sGrid.get(),
            xGrid.get(),
            yGrid.get(),
            Fscale.get(),
            Fmask.get(),
            V.get(),
            Vinv.get(),
            J.get(),
            rx.get(),
            ry.get(),
            sx.get(),
            sy.get(),
            nx.get(),
            ny.get(),
            Dr.get(),
            Ds.get(),
            Lift.get(),
            vmapM.get(),
            vmapP.get(),
            BCmap.get()
        };
    }
}