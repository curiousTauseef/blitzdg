// Copyright (C) 2017-2018  Waterloo Quantitative Consulting Group, Inc.
// See COPYING and LICENSE files at project root for more details.

#include "LinAlgHelpers.hpp"
#include "TriangleNodesProvisioner.hpp"
#include "MeshManager.hpp"
#include "Types.hpp"
#include "PathResolver.hpp"
#include <whereami.h>
#include <igloo/igloo_alt.h>
#include <blitz/array.h>
#include <iostream>
#include <limits>
#include <cmath>
#include <memory>

using boost::algorithm::find_all;
using boost::algorithm::join;
using boost::algorithm::replace_last;
using boost::algorithm::trim_right;
using boost::iterator_range;
using blitz::firstIndex;
using blitz::secondIndex;
using std::string;
using std::vector;
using std::cout;
using std::endl;
using std::numeric_limits;
using std::abs;
using std::unique_ptr;

namespace blitzdg {
    namespace TriangleNodesProvisionerTests {
        using namespace igloo;

        firstIndex ii;
        secondIndex jj;

		unique_ptr<MeshManager> meshManager = nullptr;
        unique_ptr<Nodes1DProvisioner> nodes1DProvisioner = nullptr;
		unique_ptr<TriangleNodesProvisioner> triangleNodesProvisioner = nullptr;
		unique_ptr<PathResolver> pathResolver = nullptr;

        Describe(Nodes1DProvisioner_Object) {
			const real_type eps = 50*numeric_limits<double>::epsilon();
			const float epsf = 5.8e-5;
			const index_type NOrder = 3;
			const index_type NumFaces = 2;
            string PathDelimeter = "/";

			using find_vector_type = vector<iterator_range<string::iterator>>;

            void SetUp() {
				pathResolver = unique_ptr<PathResolver>(new PathResolver());

				PathResolver& resolver = *pathResolver;

				string root = resolver.get_RootPath();
				string inputPath = resolver.joinPaths(root, "input");
				string meshPath = resolver.joinPaths(inputPath, "coarse_box.msh");

				meshManager = unique_ptr<MeshManager>(new MeshManager());
				meshManager->readMesh(meshPath);
				triangleNodesProvisioner = unique_ptr<TriangleNodesProvisioner>(new TriangleNodesProvisioner(NOrder, meshManager.get())); 
			}

			It(Should_Evaluate_Orthonormal_Simplex2D_Polynomial) {
                cout << "Should_Evaluate_Orthonormal_Simplex2D_Polynomial" << endl;
                TriangleNodesProvisioner & triangleNodes = *triangleNodesProvisioner;

				real_vector_type a(3);
				real_vector_type b(3);

				a = .1,.2,.3;
				b = .2,.3,.4;

				real_vector_type p(3);
				p = .0,.0,.0;

				triangleNodes.evaluateSimplexPolynomial(a, b, 1, 2, p);

				Assert::That(abs(p(0) - 0.133252242007405), IsLessThan(eps));
				Assert::That(abs(p(1) - 0.355359724434270), IsLessThan(eps));
				Assert::That(abs(p(2) - 0.637112282097905), IsLessThan(eps));
            }

			It(Should_Map_rs_Coords_To_ab) {
                cout << "Should_Map_rs_Coords_To_ab" << endl;
                TriangleNodesProvisioner & triangleNodes = *triangleNodesProvisioner;

				real_vector_type r(3);
				real_vector_type s(3);

				r = -.1,.1,.2;
				s = .2,.3,.5;

				real_vector_type a(3);
				real_vector_type b(3);

				triangleNodes.rsToab(r, s, a, b);

				cout << "a: " << a << endl;
				cout << "b: " << b << endl;

				Assert::That(abs(b(0) - 0.2), IsLessThan(eps));
				Assert::That(abs(b(1) - 0.3), IsLessThan(eps));
				Assert::That(abs(b(2) - 0.5), IsLessThan(eps));

				Assert::That(abs(a(0) - 1.25), IsLessThan(eps));
				Assert::That(abs(a(1) - 2.14285714285714), IsLessThan(eps));
				Assert::That(abs(a(2) - 3.8), IsLessThan(eps));
            }

			It(Should_Map_xy_Coords_To_rs) {
                cout << "Should_Map_xy_Coords_To_rs" << endl;
                TriangleNodesProvisioner & triangleNodes = *triangleNodesProvisioner;

				real_vector_type x(3), y(3), r(3), s(3);

				x = 0.5,0.6,0.7;
				y = 0.2,0.3,0.4;

				triangleNodes.xyTors(x, y, r, s);

				Assert::That(abs(r(0) -  0.0511966128287416), IsLessThan(eps));
				Assert::That(abs(r(1) -  0.0934615859097789), IsLessThan(eps));
				Assert::That(abs(r(2) -  0.1357265589908162), IsLessThan(eps));

				Assert::That(abs(s(0) - -0.1023932256574831), IsLessThan(eps));
				Assert::That(abs(s(1) -  0.0130768281804420), IsLessThan(eps));
				Assert::That(abs(s(2) -  0.1285468820183672), IsLessThan(eps));
			}

			It(Should_Build_Lifting_Operator) {
				cout << "Should_Build_Lifting_Operator" << endl;

				const int Np = (NOrder+1)*(NOrder+2)/2;
				const int totalNfp = (TriangleNodesProvisioner::NumFaces)*(NOrder+1);

		        TriangleNodesProvisioner & triangleNodes = *triangleNodesProvisioner;
				triangleNodes.buildNodes();
				triangleNodes.buildLift();

				const real_matrix_type& Lift = triangleNodes.get_Lift();

				real_matrix_type Lift_expected(Np, totalNfp);
				Lift_expected = 7.,4.04508,-1.54508,0.5,-0.833333,-4.16667,-4.16667,-0.833333,7.,4.04508,-1.54508,0.5,
							0.809017,8.,1.5,-0.309017,0.259115,2.26295,0.0719685,-0.805181,-0.292448,-2.23864,1.07038,-0.328153,
							-0.309017,1.5,8.,0.809017,-0.292448,-2.23864,1.07038,-0.328153,0.259115,2.26295,0.0719685,-0.805181,
							0.5,-1.54508,4.04508,7.,7.,4.04508,-1.54508,0.5,-0.833333,-4.16667,-4.16667,-0.833333,
							-0.292448,-2.23864,1.07038,-0.328153,-0.805181,0.0719685,2.26295,0.259115,0.809017,8.,1.5,-0.309017,
							0.0617284,-0.987654,-0.987654,0.0617284,0.0617284,-0.987654,-0.987654,0.0617284,0.0617284,-0.987654,-0.987654,0.0617284,
							-0.328153,1.07038,-2.23864,-0.292448,0.809017,8.,1.5,-0.309017,-0.805181,0.0719685,2.26295,0.259115,
							0.259115,2.26295,0.0719685,-0.805181,-0.328153,1.07038,-2.23864,-0.292448,-0.309017,1.5,8.,0.809017,
							-0.805181,0.0719685,2.26295,0.259115,-0.309017,1.5,8.,0.809017,-0.328153,1.07038,-2.23864,-0.292448,
							-0.833333,-4.16667,-4.16667,-0.833333,0.5,-1.54508,4.04508,7.,0.5,-1.54508,4.04508,7.;

				real_matrix_type res(Np, totalNfp);
				res = Lift-Lift_expected;

				cout << "Lift: " << Lift << endl;
				Assert::That(normFro(res), IsLessThan(epsf));
			}

			It(Should_Compute_Warp_Factor) {
				cout << "Should_Compute_warpFactor" << endl;
                TriangleNodesProvisioner & triangleNodes = *triangleNodesProvisioner;

				real_vector_type r(3);
				real_vector_type warp(3);
				r = -.1,.1,.2;
				warp = 0.0,0.0,0.0;

				triangleNodes.computeWarpFactor(r, warp);

				Assert::That(abs(warp(0) - -0.0384345884812357), IsLessThan(eps));
				Assert::That(abs(warp(1) -  0.0384345884812359), IsLessThan(eps));
				Assert::That(abs(warp(2) -  0.0768691769624717), IsLessThan(eps));

				cout << warp(0) << endl;
				cout << warp(1) << endl;
				cout << warp(2) << endl;
			}

			It(Should_Compute_Vandermode_Matrix) {
				cout << "Should_Compute_Vandermonde_Matrix" << endl;
                TriangleNodesProvisioner & triangleNodes = *triangleNodesProvisioner;

				const index_type Np = (NOrder+1)*(NOrder+2)/2;

				real_vector_type r(Np);
				real_vector_type s(Np);

				r = .1,.2,.3,.4,.5,.6,.7,.8,.9,1.;
				s = .1,.2,.3,.4,.5,.6,.7,.8,.9,1.;

				real_matrix_type V(Np,Np), V_expected(Np,Np);
				V = 0*jj;

				triangleNodes.computeVandermondeMatrix(NOrder, r, s, V);

				V_expected =    0.70711,0.65000,-0.45928,-0.76279,1.12583,2.41300,1.19811,1.45831,4.79915,1.83014,
								0.70711,0.80000,-0.24495,-0.90510,1.38564,3.39411,2.66504,2.40998,8.90497,4.07092,
								0.70711,0.95000,0.03062,-0.92012,1.64545,4.53432,4.82274,3.53966,14.50972,7.36686,
								0.70711,1.10000,0.36742,-0.77075,1.90526,5.83363,7.78693,4.84734,21.82920,11.89473,
								0.70711,1.25000,0.76547,-0.41984,2.16506,7.29204,11.67335,6.33304,31.07926,17.83134,
								0.70711,1.40000,1.22474,0.16971,2.42487,8.90955,16.59774,7.99675,42.47571,25.35347,
								0.70711,1.55000,1.74526,1.03503,2.68468,10.68615,22.67585,9.83847,56.23439,34.63793,
								0.70711,1.70000,2.32702,2.21324,2.94449,12.62186,30.02340,11.85819,72.57111,45.86149,
								0.70711,1.85000,2.97001,3.74148,3.20429,14.71666,38.75613,14.05593,91.70170,59.20097,
								0.70711,2.00000,3.67423,5.65685,-0.00000,-0.00000,-0.00000,0.00000,0.00000,-0.00000;

				real_matrix_type res(Np,Np);
				res = V-V_expected;
				Assert::That(normFro(res), IsLessThan(epsf));
			}

			It(Should_Compute_Grad_Vandermonde_Matrices) {

				cout << "Should_Compute_Grad_Vandermonde_Matrices" << endl;
                TriangleNodesProvisioner & triangleNodes = *triangleNodesProvisioner;

				const index_type Np = (NOrder+1)*(NOrder+2)/2;

				real_vector_type r(Np);
				real_vector_type s(Np);

				r = .1,.2,.3,.4,.5,.6,.7,.8,.9,1.;
				s = .1,.2,.3,.4,.5,.6,.7,.8,.9,1.;

				real_matrix_type VDr(Np,Np), VDrexpected(Np,Np);
				real_matrix_type VDs(Np,Np), VDsexpected(Np,Np), res(Np,Np);

				VDr = 0*jj; 
				VDs = 0*jj;

				triangleNodes.computeGradVandermondeMatrix(NOrder, r, s, VDr, VDs);

				cout << "VDr: " << VDr << endl;
				cout << "VDs: " << VDs << endl;

				VDrexpected =   0.00000,0.00000,-0.00000,-0.00000,1.73205,3.71231,1.84324,5.34029,17.57436,10.71985,
								0.00000,0.00000,-0.00000,-0.00000,1.73205,4.24264,3.33131,6.57267,24.28629,17.06196,
								0.00000,0.00000,0.00000,-0.00000,1.73205,4.77297,5.07657,7.80505,31.99434,24.63881,
								0.00000,0.00000,0.00000,-0.00000,1.73205,5.30330,7.07903,9.03742,40.69851,33.45042,
								0.00000,0.00000,0.00000,-0.00000,1.73205,5.83363,9.33868,10.26980,50.39880,43.49677,
								0.00000,0.00000,0.00000,0.00000,1.73205,6.36396,11.85553,11.50217,61.09520,54.77786,
								0.00000,0.00000,0.00000,0.00000,1.73205,6.89429,14.62958,12.73455,72.78773,67.29371,
								0.00000,0.00000,0.00000,0.00000,1.73205,7.42462,17.66082,13.96693,85.47637,81.04430,
								0.00000,0.00000,0.00000,0.00000,1.73205,7.95495,20.94926,15.19930,99.16112,96.02964,
								0.00000,0.00000,0.00000,0.00000,1.73205,8.48528,24.49490,-0.00000,-0.00000,0.00000;

				VDsexpected =   0.00000,1.50000,1.83712,-1.93570,0.86603,5.30330,9.75815,3.28634,16.70868,7.00158,
								0.00000,1.50000,2.44949,-0.84853,0.86603,6.36396,14.59896,3.83406,23.90682,10.32697,
								0.00000,1.50000,3.06186,0.60988,0.86603,7.42462,20.33995,4.38178,32.26709,14.18556,
								0.00000,1.50000,3.67423,2.43952,0.86603,8.48528,26.98113,4.92950,41.78950,18.57733,
								0.00000,1.50000,4.28661,4.64039,0.86603,9.54594,34.52250,5.47723,52.47404,23.50229,
								0.00000,1.50000,4.89898,7.21249,0.86603,10.60660,42.96405,6.02495,64.32073,28.96043,
								0.00000,1.50000,5.51135,10.15582,0.86603,11.66726,52.30579,6.57267,77.32955,34.95176,
								0.00000,1.50000,6.12372,13.47038,0.86603,12.72792,62.54772,7.12039,91.50050,41.47627,
								0.00000,1.50000,6.73610,17.15618,0.86603,13.78858,73.68984,7.66812,106.83360,48.53397,
								0.00000,1.50000,7.34847,21.21320,0.86603,4.24264,12.24745,0.00000,0.00000,0.00000;


				res = VDrexpected - VDr;
				Assert::That(normFro(res), IsLessThan(epsf));

				res = VDsexpected - VDs;
				Assert::That(normFro(res), IsLessThan(epsf));
			}

			It(Should_Compute_Differentiation_Matrices) {
				cout << "Should_Compute_Differentiation_Matrices" << endl;

                TriangleNodesProvisioner & triangleNodes = *triangleNodesProvisioner;

				const index_type Np = (NOrder+1)*(NOrder+2)/2;

				real_matrix_type V(Np,Np), V2Dr(Np,Np), V2Ds(Np,Np), Dr(Np,Np), Ds(Np,Np), 
					expectedDr(Np,Np), expectedDs(Np,Np), res(Np,Np);

				V2Dr = 0*jj; V2Ds = 0*jj; V = 0*jj; Dr = 0*jj; Ds = 0*jj;

				real_vector_type x(Np), y(Np), r(Np), s(Np);

				triangleNodes.computeEquilateralNodes(x, y);
				triangleNodes.xyTors(x, y, r, s);

				triangleNodes.computeVandermondeMatrix(NOrder, r, s, V);
				triangleNodes.computeGradVandermondeMatrix(NOrder, r, s, V2Dr, V2Ds);
				triangleNodes.computeDifferentiationMatrices(V2Dr, V2Ds, V, Dr, Ds);

				cout << "Dr:" << endl << Dr << endl;
				cout << "Ds:" << endl << Ds << endl;

				expectedDr =-3.00000,4.04508,-1.54508,0.50000,-0.00000,-0.00000,-0.00000,-0.00000,-0.00000,0.00000,
							-0.80902,0.00000,1.11803,-0.30902,-0.00000,-0.00000,0.00000,-0.00000,0.00000,0.00000,
							0.30902,-1.11803,-0.00000,0.80902,-0.00000,-0.00000,0.00000,-0.00000,0.00000,0.00000,
							-0.50000,1.54508,-4.04508,3.00000,-0.00000,0.00000,-0.00000,-0.00000,-0.00000,0.00000,
							-0.70902,1.61803,-1.30902,0.60000,-2.00000,2.70000,-0.61803,-0.19098,-0.19098,0.10000,
							0.33333,-0.62113,0.62113,-0.33333,-0.72723,0.00000,0.72723,-0.10610,0.10610,0.00000,
							-0.60000,1.30902,-1.61803,0.70902,0.61803,-2.70000,2.00000,0.19098,0.19098,-0.10000,
							0.40902,-0.19098,-0.61803,0.60000,-1.30902,2.70000,-1.30902,-2.00000,1.61803,0.10000,
							-0.60000,0.61803,0.19098,-0.40902,1.30902,-2.70000,1.30902,-1.61803,2.00000,-0.10000,
							-0.50000,-0.00000,-0.00000,0.50000,1.54508,0.00000,-1.54508,-4.04508,4.04508,-0.00000;

				expectedDs =-3.00000,0.00000,0.00000,0.00000,4.04508,0.00000,-0.00000,-1.54508,0.00000,0.50000,
							-0.70902,-2.00000,-0.19098,0.10000,1.61803,2.70000,-0.19098,-1.30902,-0.61803,0.60000,
							0.40902,-1.30902,-2.00000,0.10000,-0.19098,2.70000,1.61803,-0.61803,-1.30902,0.60000,
							-0.50000,1.54508,-4.04508,-0.00000,-0.00000,0.00000,4.04508,0.00000,-1.54508,0.50000,
							-0.80902,0.00000,0.00000,-0.00000,0.00000,-0.00000,0.00000,1.11803,-0.00000,-0.30902,
							0.33333,-0.72723,-0.10610,0.00000,-0.62113,0.00000,0.10610,0.62113,0.72723,-0.33333,
							-0.60000,1.30902,-1.61803,-0.10000,0.61803,-2.70000,2.00000,0.19098,1.30902,-0.40902,
							0.30902,-0.00000,0.00000,-0.00000,-1.11803,-0.00000,-0.00000,0.00000,0.00000,0.80902,
							-0.60000,0.61803,0.19098,-0.10000,1.30902,-2.70000,0.19098,-1.61803,2.00000,0.70902,
							-0.50000,0.00000,-0.00000,-0.00000,1.54508,0.00000,-0.00000,-4.04508,0.00000,3.00000;

				res = Dr - expectedDr;
				Assert::That(normFro(res), IsLessThan(epsf));

				res = Ds - expectedDs;
				Assert::That(normFro(res), IsLessThan(epsf));
			}

			It(Should_Compute_Equilateral_Nodes) {
				cout << "Should_Compute_Equilateral_Nodes" << endl;

				TriangleNodesProvisioner & triangleNodes = *triangleNodesProvisioner;

				const index_type Np = (NOrder+1)*(NOrder+2)/2;

				real_vector_type x(Np), y(Np), expectedx(Np), expectedy(Np);
				triangleNodes.computeEquilateralNodes(x, y);

				expectedx = -1.000000000000000,
							-0.447213595499958,
							 0.447213595499958,
							 1.000000000000000,
							-0.723606797749979,
							-0.000000000000000,
							 0.723606797749979,
							-0.276393202250021,
							 0.276393202250021,
							 0.000000000000000;

				expectedy = -5.77350269189626e-01,
							-5.77350269189626e-01,
							-5.77350269189626e-01,
							-5.77350269189626e-01,
							-9.86232000259289e-02,
							-9.67499238300623e-17,
							-9.86232000259288e-02,
							 6.75973469215554e-01,
							 6.75973469215555e-01,
							 1.15470053837925e+00;

				real_vector_type resx(Np), resy(Np);

				resx = x - expectedx;
				resy = y - expectedy;

				Assert::That(normInf(resx), IsLessThan(eps));
				Assert::That(normInf(resy), IsLessThan(eps));
			}

			It(Should_Compute_Gradient_Of_Simplex_Polynomials) {
				cout << "Should_Compute_Gradient_Of_Simplex_Polynomials" << endl;

				TriangleNodesProvisioner & triangleNodes = *triangleNodesProvisioner;

				const index_type Np = (NOrder+1)*(NOrder+2)/2;

				real_vector_type a(Np), b(Np), dpdr(Np), dpds(Np), x(Np), y(Np), r(Np), s(Np);

				triangleNodes.computeEquilateralNodes(x, y);

				triangleNodes.xyTors(x, y, r, s);

				triangleNodes.rsToab(r, s, a, b);

				triangleNodes.evaluateGradSimplex(a, b, 1, 2, dpdr, dpds);

				Assert::That(dpdr(0) -  2.44948974278318, IsLessThan(eps));
				Assert::That(dpdr(1) -  2.44948974278318, IsLessThan(eps));
				Assert::That(dpdr(2) -  2.44948974278318, IsLessThan(eps));
				Assert::That(dpdr(3) -  2.44948974278318, IsLessThan(eps));
				Assert::That(dpdr(4) - -1.74516635192836, IsLessThan(eps));
				Assert::That(dpdr(5) - -1.63299316185545, IsLessThan(eps));
				Assert::That(dpdr(6) - -1.74516635192836, IsLessThan(eps));
				Assert::That(dpdr(7) -  8.11383968316462, IsLessThan(eps));
				Assert::That(dpdr(8) -  8.11383968316462, IsLessThan(eps));
				Assert::That(dpdr(9) -  24.49489742783178, IsLessThan(eps));

				Assert::That(dpds(0) -  15.921683328090657, IsLessThan(eps));
				Assert::That(dpds(1) -  7.797415561453585, IsLessThan(eps));
				Assert::That(dpds(2) - -5.347925818670407, IsLessThan(eps));
				Assert::That(dpds(3) - -13.472193585307480, IsLessThan(eps));
				Assert::That(dpds(4) - -0.525635522272996, IsLessThan(eps));
				Assert::That(dpds(5) - -0.816496580927726, IsLessThan(eps));
				Assert::That(dpds(6) - -1.219530829655363, IsLessThan(eps));
				Assert::That(dpds(7) - -2.168803194788500, IsLessThan(eps));
				Assert::That(dpds(8) -  10.282642877953123, IsLessThan(eps));
				Assert::That(dpds(9) -  12.247448713915887, IsLessThan(eps));
			}

			It(Should_Build_Volume_To_Face_Maps) {
				cout << "Should_Build_Volume_To_Face_Maps" << endl;
				TriangleNodesProvisioner & triangleNodes = *triangleNodesProvisioner;

				triangleNodes.buildNodes();
				triangleNodes.buildPhysicalGrid();
				triangleNodes.buildMaps();
			}
		};
   } // namespace Nodes1DProvisionerTests
} // namespace blitzdg
