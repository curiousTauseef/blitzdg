
#include "Poisson2DSparseMatrix.hpp"
#include "SparseTriplet.hpp"
#include "CSCMatrix.hpp"
#include "Nodes1DProvisioner.hpp"
#include "MeshManager.hpp"
#include "VandermondeBuilders.hpp"
#include "BCtypes.hpp"
#include "BlitzHelpers.hpp"
#include "Types.hpp"

#include <memory>

using blitz::Range;
using boost::python::numpy::ndarray;
using boost::python::numpy::zeros;
using boost::python::numpy::dtype;
using boost::python::dict;

namespace blitzdg{
    Poisson2DSparseMatrix::Poisson2DSparseMatrix(DGContext2D& dg, MeshManager& mshManager) {
        const index_vector_type& bcType = mshManager.get_BCType();
        buildPoissonOperator(dg, mshManager, bcType);
    }

    void Poisson2DSparseMatrix::buildBcRhs(DGContext2D& dg, const MeshManager& mshManager, const real_matrix_type& ubc, const real_matrix_type& qbc, const index_vector_type& bcType) {
        const index_type Np = dg.numLocalPoints();
        const index_type K = dg.numElements();
        const index_type Nfaces = dg.numFaces();
        const index_type Nfp = dg.numFacePoints();

        real_matrix_type bcRhs(Np, K);
        bcRhs = 0.0;

        const real_matrix_type& Dr = dg.Dr();
        const real_matrix_type& Ds = dg.Ds();

        const real_matrix_type& rx = dg.rx();
        const real_matrix_type& ry = dg.ry();
        const real_matrix_type& sx = dg.sx();
        const real_matrix_type& sy = dg.sy();
        const real_matrix_type& J  = dg.jacobian();
        const real_matrix_type& Fscale = dg.fscale();

        const index_vector_type& EToE = mshManager.get_EToE();
        const index_vector_type& EToF = mshManager.get_EToF();

        blitz::firstIndex ii;
        blitz::secondIndex jj;
        blitz::thirdIndex kk;

        const index_type N = dg.order();

        const index_vector_type& vmapM = dg.vmapM();
        const index_vector_type& vmapP = dg.vmapP();

        const index_matrix_type& Fmask = dg.fmask();

        real_tensor3_type massEdge(Np, Np, Nfaces);
        massEdge = 0.0;

        Nodes1DProvisioner nodes1d(Nfp-1, 1, -1., 1.);

        real_matrix_type V1D1(Nfp, Nfp), V1Dinv1(Nfp, Nfp);
        real_matrix_type V1D2(Nfp, Nfp), V1Dinv2(Nfp, Nfp);
        real_matrix_type V1D3(Nfp, Nfp), V1Dinv3(Nfp, Nfp);

        real_matrix_type myTemp(Nfp, Nfp);

        VandermondeBuilders vandermonde;

        const real_vector_type& r = dg.r(), s = dg.s();
        const real_matrix_type& nx = dg.nx(), ny  = dg.ny();

        real_vector_type edge1(Nfp), edge2(Nfp), edge3(Nfp);
        for (index_type i=0; i < Nfp; ++i) {
            edge1(i) = r(Fmask(i, 0));
        }

        vandermonde.computeVandermondeMatrix(edge1, V1D1, V1Dinv1);
        myTemp = blitz::sum(V1Dinv1(kk,ii)*V1Dinv1(kk,jj), kk);

        for (index_type i=0; i < Nfp; ++i) {
            for (index_type j=0; j < Nfp; ++j) {
                massEdge(Fmask(i, 0), Fmask(j, 0), 0) = myTemp(i, j);
            }
        }

        for (index_type i=0; i < Nfp; ++i) {
            edge2(i) = r(Fmask(i, 1));
        }

        vandermonde.computeVandermondeMatrix(edge2, V1D2, V1Dinv2);
        myTemp = blitz::sum(V1Dinv2(kk,ii)*V1Dinv2(kk,jj), kk);
        for (index_type i=0; i < Nfp; ++i) {
            for (index_type j=0; j < Nfp; ++j) {
                massEdge(Fmask(i, 1), Fmask(j, 1), 1) = myTemp(i, j);
            }
        }

        for (index_type i=0; i < Nfp; ++i) {
            edge3(i) = s(Fmask(i, 2));
        }

        vandermonde.computeVandermondeMatrix(edge3, V1D3, V1Dinv3);
        myTemp = blitz::sum(V1Dinv3(kk,ii)*V1Dinv3(kk,jj), kk);
        for (index_type i=0; i < Nfp; ++i) {
            for (index_type j=0; j < Nfp; ++j) {
                massEdge(Fmask(i, 2), Fmask(j, 2), 2) = myTemp(i, j);
            }
        }

		real_vector_type ubcCol(Nfp*Nfaces*K), qbcCol(Np*Nfaces*K);
		ubcCol = 0.0;
		qbcCol = 0.0;
		reshapeMatTo1D(ubc, ubcCol.data(), false);
		reshapeMatTo1D(qbc, qbcCol.data(), false);

        for (index_type k=0; k < K; ++k) {
            const index_type k1 = k;

            real_matrix_type Dx(Np, Np), Dy(Np, Np);

            Dx = rx(0, k)*Dr + sx(0, k)*Ds;
            Dy = ry(0, k)*Dr + sy(0, k)*Ds;

            for (index_type f1=0; f1 < Nfaces; ++f1) {
                index_type k2 = EToE(Nfaces*k1 + f1);
                index_type f2 = EToF(Nfaces*k1 + f1);

                index_vector_type vidM(Nfp), vidP(Nfp),
                    Fm1(Nfp), Fm2(Nfp);

                real_vector_type ubcf1(Nfp), qbcf1(Nfp);
				ubcf1 = 0.0;
				qbcf1 = 0.0;

                for (index_type j=0; j < Nfp; ++j) {
                    index_type fidM = Nfp*Nfaces*k + f1*Nfp + j;
                    vidM(j) = vmapM(fidM);
                    vidP(j) = vmapP(fidM);
                    Fm1(j) = vidM(j) % Np;
                    Fm2(j) = vidP(j) % Np;

                    ubcf1(j) = ubcCol(fidM);
                    qbcf1(j) = qbcCol(fidM);
                }

                real_type lnx = nx(f1*Nfp, k1), lny = ny(f1*Nfp, k1),
                      lsJ = Fscale(f1*Nfp, k1)*J(Fm1(1), k1);

                real_type hinv = std::max(Fscale(f1*Nfp, k1), Fscale(f2*Nfp, k2));
                real_matrix_type Dx2(Np, Np), Dy2(Np, Np);

                Dx2 = rx(0, k2)*Dr + sx(0, k2)*Ds;
                Dy2 = ry(0, k2)*Dr + sy(0, k2)*Ds;

                real_matrix_type Dn1(Np, Np), Dn2(Np, Np);
                Dn1 = lnx*Dx  + lny*Dy;
                Dn2 = lnx*Dx2 + lny*Dy2;

                real_matrix_type mmE(Np, Np), mmE_Fm1(Np, Nfp);
                mmE = lsJ * massEdge(Range::all(), Range::all(), f1);

				for (index_type j=0; j < Nfp; ++j) {
					mmE_Fm1(Range::all(), j) = mmE(Range::all(), Fm1(j));
				}

                real_type gtau = 100*100*2*(N+1)*(N+1)*hinv; // set penalty scaling

                real_vector_type tmp(Np);
                tmp = 0.0;
				real_matrix_type tmp2(Nfp, Np), tmp3(Np, Nfp);
				tmp2 = 0.0, tmp3 = 0.0;

                switch(bcType(Nfaces*k1 +f1)) {
                case BCTag::Dirichlet:
					// (gtau*mmE(:,Fm1) - Dn1'*mmE(:,Fm1))*ubc(fidM);
					tmp3  = -blitz::sum(Dn1(kk,ii)*mmE_Fm1(kk,jj), kk);
					tmp3 += gtau*mmE_Fm1;

                    tmp = blitz::sum(tmp3(ii,jj)*ubcf1(jj), jj);
                    bcRhs(Range::all(), k1) += tmp;
                    break;
                case BCTag::Neuman:
                case BCTag::Wall:
                    tmp = blitz::sum(mmE_Fm1(ii,jj)*qbcf1(jj), jj); // need mmE along f1
					bcRhs(Range::all(), k1) += tmp;
                    break;
                }
            }
        }

        BcRhs_ = std::make_unique<real_matrix_type>(bcRhs);
    }

    void Poisson2DSparseMatrix::buildPoissonOperator(DGContext2D& dg, MeshManager& mshManager, const index_vector_type& bcType) {
        const real_matrix_type& Dr = dg.Dr();
        const real_matrix_type& Ds = dg.Ds();

        const real_matrix_type& rx = dg.rx();
        const real_matrix_type& ry = dg.ry();
        const real_matrix_type& sx = dg.sx();
        const real_matrix_type& sy = dg.sy();
        const real_matrix_type& Fscale = dg.fscale();
        const real_matrix_type& J      = dg.jacobian();

        const real_matrix_type& Vinv = dg.Vinv();

        const index_vector_type& EToE = mshManager.get_EToE();
        const index_vector_type& EToF = mshManager.get_EToF();

        blitz::firstIndex ii;
        blitz::secondIndex jj;
        blitz::thirdIndex kk;

        const index_type N = dg.order();
        const index_type Np = dg.numLocalPoints();
        const index_type K = dg.numElements();
        const index_type Nfaces = dg.numFaces();
        const index_type Nfp = dg.numFacePoints();

        const index_vector_type& vmapM = dg.vmapM();
        const index_vector_type& vmapP = dg.vmapP();

        real_matrix_type MassMatrix(Np, Np);

        MassMatrix = blitz::sum(Vinv(kk,ii)*Vinv(kk,jj),kk);

        const index_matrix_type& Fmask = dg.fmask();

        real_tensor3_type massEdge(Np, Np, Nfaces);
        massEdge = 0.0;

        Nodes1DProvisioner nodes1d(Nfp-1, 1, -1., 1.);

        real_matrix_type V1D1(Nfp, Nfp), V1Dinv1(Nfp, Nfp);
        real_matrix_type V1D2(Nfp, Nfp), V1Dinv2(Nfp, Nfp);
        real_matrix_type V1D3(Nfp, Nfp), V1Dinv3(Nfp, Nfp);

        real_matrix_type myTemp(Nfp, Nfp);

        VandermondeBuilders vandermonde;

        const real_vector_type& r = dg.r(), s = dg.s();
        const real_matrix_type& nx = dg.nx(), ny  = dg.ny();

        real_vector_type edge1(Nfp), edge2(Nfp), edge3(Nfp);
        for (index_type i=0; i < Nfp; ++i) {
            edge1(i) = r(Fmask(i, 0));
        }

        vandermonde.computeVandermondeMatrix(edge1, V1D1, V1Dinv1);
        myTemp = blitz::sum(V1Dinv1(kk,ii)*V1Dinv1(kk,jj), kk);

        for (index_type i=0; i < Nfp; ++i) {
            for (index_type j=0; j < Nfp; ++j) {
                massEdge(Fmask(i, 0), Fmask(j, 0), 0) = myTemp(i, j);
            }
        }

        for (index_type i=0; i < Nfp; ++i) {
            edge2(i) = r(Fmask(i, 1));
        }

        vandermonde.computeVandermondeMatrix(edge2, V1D2, V1Dinv2);
        myTemp = blitz::sum(V1Dinv2(kk,ii)*V1Dinv2(kk,jj), kk);
        for (index_type i=0; i < Nfp; ++i) {
            for (index_type j=0; j < Nfp; ++j) {
                massEdge(Fmask(i, 1), Fmask(j, 1), 1) = myTemp(i, j);
            }
        }

        for (index_type i=0; i < Nfp; ++i) {
            edge3(i) = s(Fmask(i, 2));
        }

        vandermonde.computeVandermondeMatrix(edge3, V1D3, V1Dinv3);
        myTemp = blitz::sum(V1Dinv3(kk,ii)*V1Dinv3(kk,jj), kk);
        for (index_type i=0; i < Nfp; ++i) {
            for (index_type j=0; j < Nfp; ++j) {
                massEdge(Fmask(i, 2), Fmask(j, 2), 2) = myTemp(i, j);
            }
        }
        
        // allocate storage for sparse matrix entries. 
        SparseTriplet MM(Np*K, Np*K, Np*Np*K),
            OP(Np*K, Np*K, (1+Nfaces)*Np*Np*K);

        index_vector_type entries(Np*Np), entriesMM(Np*Np);

        entries = ii;
        entriesMM = ii;

        for (index_type k=0; k < K; ++k) {
            index_matrix_type rows1(Np, Np), cols1(Np, Np);
            rows1 = ((ii+Np*k) * (0*jj + 1));
            cols1 = rows1(jj, ii);

            real_matrix_type Dx(Np, Np), Dy(Np, Np);

            Dx = rx(0, k)*Dr + sx(0, k)*Ds;
            Dy = ry(0, k)*Dr + sy(0, k)*Ds;

            real_matrix_type OP11(Np, Np), tmp(Np, Np);

            // 'Volume' contribution.
            tmp   = blitz::sum(Dx(kk, ii)*MassMatrix(kk, jj), kk);
            OP11  = blitz::sum(tmp(ii, kk)*Dx(kk, jj), kk);
            tmp   = blitz::sum(Dy(kk, ii)*MassMatrix(kk, jj), kk);
            OP11 += blitz::sum(tmp(ii, kk)*Dy(kk, jj), kk);
            OP11 *= J(0, k);

            const index_type k1 = k;
            for (index_type f1=0; f1 < Nfaces; ++f1) {
                index_type k2 = EToE(Nfaces*k1 + f1);
                index_type f2 = EToF(Nfaces*k1 + f1);

                index_matrix_type rows2(Np, Np), cols2(Np, Np);
                rows2 = ((ii+ Np*k2) ) * (0*jj + 1);
                cols2 = rows2(jj, ii);

                index_vector_type vidM(Nfp), vidP(Nfp),
                    Fm1(Nfp), Fm2(Nfp);

                for (index_type j=0; j < Nfp; ++j) {
                    index_type fidM = Nfp*Nfaces*k + f1*Nfp + j;
                    vidM(j) = vmapM(fidM);
                    vidP(j) = vmapP(fidM);
                    Fm1(j) = vidM(j) % Np;
                    Fm2(j) = vidP(j) % Np;
                }

                real_type lnx = nx(f1*Nfp, k1), lny = ny(f1*Nfp, k1),
                      lsJ = Fscale(f1*Nfp, k1)*J(Fm1(1), k1);

                real_type hinv = std::max(Fscale(f1*Nfp, k1), Fscale(f2*Nfp, k2));
                real_matrix_type Dx2(Np, Np), Dy2(Np, Np);

                Dx2 = rx(0, k2)*Dr + sx(0, k2)*Ds;
                Dy2 = ry(0, k2)*Dr + sy(0, k2)*Ds;

                real_matrix_type Dn1(Np, Np), Dn2(Np, Np);
                Dn1 = lnx*Dx  + lny*Dy,
                Dn2 = lnx*Dx2 + lny*Dy2;

                real_matrix_type mmE(Np, Np);
                mmE = lsJ * massEdge(Range::all(), Range::all(), f1);

                real_type gtau = 100*100*2*(N+1)*(N+1)*hinv; // set penalty scaling

                switch(bcType(Nfaces*k1 +f1)) {
                case BCTag::Dirichlet:
                    OP11 += ( gtau*mmE - blitz::sum(mmE(ii,kk)*Dn1(kk,jj), kk) - blitz::sum(Dn1(kk,ii)*mmE(kk,jj), kk) ); // ok
                    break;
                case BCTag::Neuman:
                case BCTag::Wall:
                    break;
                default:
                    // interior face variational terms
                    OP11 += 0.5*( gtau*mmE(ii,jj) - blitz::sum(mmE(ii,kk)*Dn1(kk,jj), kk) - blitz::sum(Dn1(kk,ii)*mmE(kk,jj), kk) );

                    real_matrix_type OP12(Np, Np);
                    OP12 = 0.0;

                    for (index_type i=0; i < Np; ++i) {
                        for (index_type j=0; j < Nfp; ++j) {
                            OP12(i, Fm2(j)) +=             - 0.5*( gtau*mmE(i, Fm1(j)) );  
                        }
                    }

                    // sliced temporaries.
                    real_matrix_type mmE_Fm1Fm1(Nfp, Nfp), Dn2_Fm2(Nfp, Np),
                        mmE_Fm1(Np, Nfp), prod1(Nfp, Np), prod2(Np, Nfp);

                    mmE_Fm1Fm1 = 0.0; Dn2_Fm2 = 0.0;
                    mmE_Fm1 = 0.0;
                    for (index_type i=0; i < Nfp; ++i) {
                        for(index_type j=0; j < Nfp; ++j) {
                            mmE_Fm1Fm1(i, j) = mmE(Fm1(i), Fm1(j));
                        }
                        for(index_type j=0; j < Np; ++j) {
                            Dn2_Fm2(i, j) = Dn2(Fm2(i), j);
                            mmE_Fm1(j, i) = mmE(j, Fm1(i));
                        }
                    }

                    // products needed in the face-to-neighbour variational terms assembly.
                    prod1 = blitz::sum(mmE_Fm1Fm1(ii, kk)*Dn2_Fm2(kk, jj), kk);
                    prod2 = blitz::sum(-Dn1(kk, ii)*mmE_Fm1(kk, jj), kk);

                    for (index_type i=0; i < Np; ++i) {
                        for (index_type j=0; j < Nfp; ++j) {
                            OP12(Fm1(j), i) += -0.5*prod1(j, i);
                            OP12(i, Fm2(j)) += -0.5*prod2(i, j);
                        }
                    }

                    for (index_type i=0; i < Np; ++i) {
                        for (index_type j=0; j < Np; ++j) { 
                            OP.insert(rows1(i, j), cols2(i, j), OP12(i, j));
                        }
                    }
                    entries += Np*Np;
                }
            }

            tmp = J(0, k)*MassMatrix;                    
            for (index_type i=0; i < Np; ++i ) {
                for (index_type j=0; j < Np; ++j) {
                    OP.insert(rows1(i, j), cols1(i, j), OP11(i,j));
                    MM.insert(rows1(i, j), cols1(i, j), tmp(i,j));
                }
            }
            entries += Np*Np;
            entriesMM += Np*Np;
        }

        OP_ = std::make_unique<CSCMat>(OP);
        MM_ = std::make_unique<CSCMat>(MM);
    }

    const ndarray Poisson2DSparseMatrix::buildBcRhs_numpy(DGContext2D& dg, const MeshManager& mshManager, const ndarray& ubc, const ndarray& qbc) {
        // do all the things.

        real_matrix_type blitzUbc(ubc.shape(0), ubc.shape(1));
		blitzUbc = 0.0;
		char * raw = ubc.get_data();
        std::copy(&raw[0], &raw[ubc.shape(0)*ubc.shape(1)*sizeof(real_type)] , reinterpret_cast<char*>(blitzUbc.data()));

        real_matrix_type blitzQbc(qbc.shape(0), qbc.shape(1));
		blitzQbc = 0.0;
		raw = qbc.get_data();
        std::copy(&raw[0], &raw[qbc.shape(0)*qbc.shape(1)*sizeof(real_type)] , reinterpret_cast<char*>(blitzQbc.data()));

        const index_vector_type & bcType = mshManager.get_BCType();
		buildBcRhs(dg, mshManager, blitzUbc, blitzQbc, bcType);

		return getBcRhs_numpy();
    }

    const ndarray Poisson2DSparseMatrix::getOP_numpy() const {
        Py_intptr_t shape[2] = { OP_->nnz(), 3 };
        ndarray result = zeros(2, shape, dtype::get_builtin<real_type>());

        real_type *raw = reinterpret_cast<real_type*>(result.get_data());
        
        index_type count = 0;
        for (index_type j = 0; j < OP_->cols(); ++j) {
            for (auto k = OP_->colPtrs(j); k < OP_->colPtrs(j + 1); ++k) {
                raw[3*count]   = OP_->rowInds(k);
                raw[3*count+1] = j;
                raw[3*count+2] = OP_->elems(k);

                ++count;
            }
        }
        return result;
    }

    const ndarray Poisson2DSparseMatrix::getMM_numpy() const {
        Py_intptr_t shape[2] = { MM_->nnz(), 3 };
        ndarray result = zeros(2, shape, dtype::get_builtin<real_type>());

        real_type *raw = reinterpret_cast<real_type*>(result.get_data());
        
        index_type count = 0;
        for (index_type j = 0; j < MM_->cols(); ++j) {
            for (auto k = MM_->colPtrs(j); k < MM_->colPtrs(j + 1); ++k) {
                raw[3*count]   = MM_->rowInds(k);
                raw[3*count+1] = j;
                raw[3*count+2] = MM_->elems(k);
                ++count;
            }
        }
        return result;
    }

	const ndarray Poisson2DSparseMatrix::getBcRhs_numpy() const {
		Py_intptr_t shape[2] = { BcRhs_->rows(), BcRhs_->cols() };
		ndarray result = zeros(2, shape, dtype::get_builtin<real_type>());

        std::copy(BcRhs_->begin(), BcRhs_->end(), reinterpret_cast<real_type*>(result.get_data()));
        return result; 
	}
}
