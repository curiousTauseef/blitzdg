/**
 * @file GMRESSolver.hpp
 * @brief Implements a right-preconditioned GMRES iterative method.
 */
#pragma once
#include "LinAlgHelpers.hpp"
#include "Types.hpp"
#include <algorithm>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <ostream>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

namespace blitzdg {
    namespace details {
        /**
         * Calls the BLAS function dtrsv to solve the triangular
         * linear system \f$Uy = x\f$, where \f$U\f$ is the upper
         * triangular part of the \f$n\times n\f$ leading principal
         * submatrix of \f$A\f$.
         * @param[in] n The order of \f$U\f$.
         * @param[in] A The dense matrix. Note: A must be stored in column-major format.
         * @param[in,out] x The rhs on input. The solution on output.
         */
        void backSolve(index_type n, matrix_type& A, vector_type& x);

        /**
         * Calls the BLAS function dgemv to compute the matrix-vector 
         * product \f$Ax\f$.
         * @param[in] n The number of columns of A to use in the product.
         * @param[in] A The dense matrix. Note: A must be stored in column-major format.
         * @param[in] x The real-valued vector.
         * @param[out] result The computed product \f$Ax\f$.
         */
        void matTimesVec(index_type n, matrix_type& A, vector_type& x, vector_type& result);

        /**
         * Returns true if the input array x is not uniquely zero.
         */ 
        inline bool isNonzero(const vector_type& x) {
            return std::any_of(x.begin(), x.end(),
                [](real_type val) { return val != real_type(0); });
        }
    } // namespace details

    /**
     * Convergence flags for GMRESSolver.
     */
    enum class ConvFlag { 
        /** neither converged nor diverged */
        unconverged,  
        /** converged with residual norm <= convTol */
        success, 
        /** diverged with residual norm >= divTol */    
        diverged,    
        /** maximum iterations reached */ 
        maxits, 
        /** insufficient change in solution */      
        stagnation,
        /** input matrix or preconditioner are (likely) singular */   
        breakdown, 
        /** true residual norm > convTol */   
        true_rnrm,    
        /** residual norm is inf or nan */
        inf_or_nan,   
        /** application of preconditioner failed */
        precon_fail,  
        /** matrix-vector product failed */
        matvec_fail   
    };

    /**
     * Convert a ConvFlag to a string.
     */
    std::string ConvFlagToStr(ConvFlag flag);

    /**
     * Write a ConvFlag to an output stream.
     */
    inline std::ostream& operator<<(std::ostream& strm, ConvFlag flag) {
        strm << ConvFlagToStr(flag);
        return strm;
    }

    /**
     * Input parameters for the GMRES solver.
     */
    struct GMRESParams {
        bool verbose;        /**< if true, output convergence history to console */
        bool testTrueRnrm;   /**< if true, check the true residual norm for convergence */
        index_type kspaceSz; /**< max dimension of the Krylov subspace */
        index_type maxits;   /**< max number of outer iterations */
        real_type relTol;    /**< relative convergence tolerance */
        real_type absTol;    /**< absolute convergence tolerance */
        real_type divTol;    /**< divergence tolerance */
        real_type stgTol;    /**< stagnation tolerance */
        GMRESParams()
            : verbose{ false }, testTrueRnrm{ true }, 
            kspaceSz{ 30 }, maxits{ 100 }, relTol{ 1e-6 }, 
            absTol{ 1e-6 }, divTol{ 1e5 }, stgTol{ 1e-12 }
        {}
    };

    /**
     * Checks the input parameters in GMRESParams and throws an 
     * std::invalid_argument exception if any are out of range.
     */ 
    void checkGMRESParams(const GMRESParams& p);

    /**
     * GMRES output information.
     */
    struct GMRESOut {
        ConvFlag flag;       /**< convergence flag */
        index_type innerIts; /**< inner iteration number at which solution was computed */
        index_type outerIts; /**< outer iteration number at which solution was computed */
        real_type relres;    /**< relative residual norm of solution */
        std::string msg;     /**< addtional output info */
        GMRESOut()
            : flag{ ConvFlag::unconverged },
            innerIts{ 0 }, outerIts{ 0 }, relres{ real_type(0) },
            msg{}
        {}
    };

    /**
     * Write a GMRES output to an output stream. 
     */
    std::ostream& operator<<(std::ostream& strm, const GMRESOut& out);

    /**
     * Preconditioned, restarted GMRES with Modified
     * Gram-Schmidt orthogonalization.
     * 
     * Solves the right-preconditioned linear system \f$AM^{-1}u = b\f$, 
     * where \f$u = Mx\f$ and \f$M\f$ is the preconditioner. 
     * 
     * <b>Stopping Criteria<\b>
     * 
     * The solver iterates until one of the following criteria are met:
     * 
     * <ul>
     * <li> The convergence criterion is met: 
     * \f$\|r_k\| \leq \max(\text{relTol}\cdot\|b\|, \text{absTol})\f$
     * The solver terminates with convergence flag ConvFlag::success.
     * 
     * <li> The divergence criterion is met:
     * \f$\|r_k\| \geq \text{divTol}\cdot\|r_0\|\f$
     * The solver terminates with convergence flag ConvFlag::diverged.
     * 
     * <li> The stagnation criterion is met:
     * \f$|x_{k+1}(i) - x_k(i)| \leq \text{stgTol}\cdot|x_{k+1}(i)|~~\forall i\f$
     * The solver terminates with convergence flag ConvFlag::stagnation.
     * 
     * <li> The maximum number of iterations \f$\text{maxits}\cdot\text{kspaceSz}\f$ is reached.
     * The solver terminates with convergence flag ConvFlag::maxits.
     * <\ul>
     * 
     * The norm \f$\|\cdot\|\f$ is always the vector 2-norm and \f$r_k\f$ is
     * the unpreconditioned residual of the \f$k\f$th iterate \f$x_k\f$.
     * 
     * Various other factors may cause the solver to terminate. These include:
     * 
     * <ul>
     * <li> The input matrix or the preconditioner are singular. 
     * The solver termintes with convergence flag ConvFlag::breakdown.
     * 
     * <li> The residual norm is inf or nan.
     * The solver terminates with convergence flag ConvFlag::inf_or_nan.
     * 
     * <li> The matrix-vector multiply fails.
     * The solver terminates with convergence flag ConvFlag::matvec_fail.
     * 
     * <li> Application of the preconditioner fails.
     * The solver terminates with convergence flag ConvFlag::precon_fail.
     * <\ul>
     */
    class GMRESSolver {
    public:
        /**
         * Calls GMRES to solves the linear system \f$Ax = b\f$.
         * @param[in] mvec is a functor that computes the matrix-vector product,
         * i.e., it computes \f$Ax\f$. It must define an overload of operator() 
         * with the signature:
         * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
         * bool operator()(const vector_type& in, vector_type& out)
         * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
         * which returns true if successful.
         * @param[in] precon is a functor that applies the preconditioner
         * to a vector, i.e., it computes \f$M^{-1}x\f$.
         * It must define an overload of operator() with the signature:
         * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
         * bool operator()(const vector_type& in, vector_type& out)
         * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
         * which returns true if successful.
         * @param[in] b The rhs of the linear system.
         * @param[in,out] x The initial guess on input and the solution on output
         * @param[in] params The input parameters for GMRES.
         * @return The convergence information.
         */
        template <typename Matvec, typename Precon>
        GMRESOut solve(Matvec mvec, Precon precon, const vector_type& b,
            vector_type& x, const GMRESParams& params = GMRESParams()) const;
    };

    template <typename Matvec, typename Precon>
    GMRESOut GMRESSolver::solve(Matvec mvec, Precon precon, const vector_type& b,
            vector_type& x, const GMRESParams& params) const
    {
        checkGMRESParams(params); // check for any errors in input parameters
        const index_type N = x.size();
        const index_type kspaceSz = std::min(N, params.kspaceSz);
        vector_type r(N), w(N), pv(N), s(kspaceSz + 1);
        matrix_type H(kspaceSz + 1, kspaceSz, blitz::ColumnMajorArray<2>());
        matrix_type V(N, kspaceSz + 1, blitz::ColumnMajorArray<2>());
        std::vector<std::pair<real_type, real_type>> givens(kspaceSz);
        GMRESOut ret;
        r = b;

        real_type bnrm = norm2(b);
        real_type rnrm = bnrm;
        if (bnrm == real_type(0)) {
            bnrm = real_type(1);
        }
        const real_type bnrmInv = real_type(1) / bnrm;
        
        // if initial guess is nonzero, update residual
        if (details::isNonzero(x)) {
            if (!mvec(x, w)) {
                ret.flag = ConvFlag::matvec_fail;
                ret.msg = "matvec failed when computing initial residual";
                return ret;
            }
            r -= w;
            rnrm = norm2(r);
        }

        // check for inf or nan
        if (!std::isfinite(rnrm)) {
            ret.flag = ConvFlag::inf_or_nan;
            return ret;
        }

        // define convergence and divergence tolerances
        const real_type convTol = std::max(params.relTol * bnrm, params.absTol);
        const real_type divTol = params.divTol * rnrm;
        
        // check for early termination
        if (rnrm <= convTol) {
            ret.flag = ConvFlag::success;
            ret.relres = bnrmInv * rnrm;
            return ret;
        }

        if (params.verbose) {
            std::cout << "inner iter  outer iter  relative rnrm  conv rate\n";
            std::cout << std::setw(10) << 0 << "  "
                << std::setw(10) << 0 << "  "
                << std::scientific << std::setprecision(2)
                << std::setw(13) << bnrmInv * rnrm << "  "
                << std::scientific << std::setprecision(2)
                << std::setw(9) << 1 << std::endl;
        }

        // time to grip it and rip it
        bool testTrueRnrm = params.testTrueRnrm;
        for (index_type k = 0; k < params.maxits; ++k) {
            ret.relres = bnrmInv * rnrm;
            ret.outerIts = k;
            real_type scal = real_type(1) / rnrm;
            V(blitz::Range::all(), 0) = r;
            V(blitz::Range::all(), 0) *= scal;
            s = real_type(0);
            s(0) = rnrm;
            index_type iupdate = kspaceSz;
            for (index_type i = 0; i < kspaceSz; ++i) {
                ret.innerIts = i;
                real_type rnrmold = rnrm;
                vector_type viref = V(blitz::Range::all(), i); // ref to slice
                if (!precon(viref, pv)) { // pv = M^{-1}*V(:,i)
                    ret.flag = ConvFlag::precon_fail;
                    ret.msg = "preconditioner application in inner loop failed";
                    return ret;
                } 
                if (!mvec(pv, w)) { // w = A*pv = A*M^{-1}*V(:,i)
                    ret.flag = ConvFlag::matvec_fail;
                    ret.msg = "matvec in inner loop failed";
                    return ret;
                }
                // MGS loop
                for (index_type j = 0; j <= i; ++j) {
                    real_type hji = sum(w * V(blitz::Range::all(), j));
                    w -= hji * V(blitz::Range::all(), j);
                    H(j, i) = hji;
                }
                H(i + 1, i) = norm2(w);
                
                // If H(i + 1, i) = 0, then the ith Givens rotation J_i
                // is equal to the identity matrix and hence the 
                // (i + 1)th element of J_i*s is equal to zero, which 
                // will trigger termination of the inner loop.
                if (H(i + 1, i) != real_type(0)) {
                    V(blitz::Range::all(), i + 1) = w;
                    scal = real_type(1) / H(i + 1, i);
                    V(blitz::Range::all(), i + 1) *= scal;
                }
                
                // apply Givens rotations 0,...,i-1 to column i of H
                for (index_type j = 0; j < i; ++j) {
                    applyGivens(givens[j].first, givens[j].second, H(j, i), H(j + 1, i));
                }
                
                // compute the ith Givens rotation and apply it to column i of H
                DROTG(H(i, i), H(i + 1, i), givens[i].first, givens[i].second);
                
                // check if H is singular, i.e., H(i,i) = 0
                if (H(i, i) == real_type(0)) {
                    ret.flag = ConvFlag::breakdown;
                    std::ostringstream ss;
                    ss << "H(" << i << "," << i << ") = 0";
                    ret.msg = ss.str();
                    return ret;
                }
                
                // apply the ith Givens rotation to s and get the approximate residual norm
                applyGivens(givens[i].first, givens[i].second, s(i), s(i + 1));
                rnrm = std::abs(s(i + 1));

                // check for inf or nan
                if (!std::isfinite(rnrm)) {
                    ret.flag = ConvFlag::inf_or_nan;
                    return ret;
                }

                if (params.verbose) {
                    std::cout << std::setw(10) << k + 1 << "  "
                        << std::setw(10) << i + 1 << "  "
                        << std::scientific << std::setprecision(2)
                        << std::setw(13) << bnrmInv * rnrm << "  "
                        << std::scientific << std::setprecision(2)
                        << std::setw(9) << rnrm / rnrmold << std::endl;
                    rnrmold = rnrm;
                }

                // check for convergence 
                if (rnrm <= convTol) {
                    ret.flag = ConvFlag::success;
                    iupdate = i + 1;
                    break;
                }

                // check for divergence
                if (rnrm >= divTol) {
                    ret.flag = ConvFlag::diverged;
                    iupdate = i + 1;
                    break;
                }
            }
            // update the solution
            // solve H(0:iupdate-1,0:iupdate-1)*y = s(0:iupdate-1)
            details::backSolve(iupdate, H, s);
            // compute w = V(:,0:iupdate-1)*y
            details::matTimesVec(iupdate, V, s, w);
            // compute pv = M^{-1}*w
            if (!precon(w, pv)) {
                ret.flag = ConvFlag::precon_fail;
                ret.msg = "preconditioner application failed when updating solution";
                return ret;
            }
            // update x
            x += pv;
            
            // if termination signaled in inner loop, then exit
            if (ret.flag != ConvFlag::unconverged)
                break;

            // update the residual 
            if (!mvec(x, w)) {
                ret.flag = ConvFlag::matvec_fail;
                ret.msg = "matvec failed when updating residual";
                return ret;
            }
            r = b - w;
            rnrm = norm2(r);

            // check for inf or nan
            if (!std::isfinite(rnrm)) {
                ret.flag = ConvFlag::inf_or_nan;
                return ret;
            }

            // check for convergence
            if (rnrm <= convTol) {
                ret.flag = ConvFlag::success;
                testTrueRnrm = false;
                break;
            }
            
            // check for divergence
            if (rnrm >= divTol) {
                ret.flag = ConvFlag::diverged;
                break;
            }

            // check for stagnation
            bool stagnated = true;
            for (index_type j = 0; j < N; ++j) {
                if (x(j) != real_type(0) && 
                std::abs(pv(j)) > params.stgTol * std::abs(x(j))) 
                {
                    stagnated = false;
                    break;
                }
            }
            if (stagnated) {
                ret.flag = ConvFlag::stagnation;
                break;
            }
        }
        if (ret.flag == ConvFlag::unconverged)
            ret.flag = ConvFlag::maxits;
        
        // if requested, check that the true residual norm satisfies
        // the convergence criteria
        if (ret.flag == ConvFlag::success && testTrueRnrm) {
            if (mvec(x, w)) {
                r = b - w;
                rnrm = norm2(r);
                if (std::isfinite(rnrm) && rnrm > convTol)
                    ret.flag = ConvFlag::true_rnrm;
            }
            else
                ret.msg =  "matvec failed when computing true residual -> test skipped";
        }
        ret.relres = bnrmInv * rnrm;
        return ret;
    }
} // namespace blitzdg