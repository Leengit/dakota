/*  _______________________________________________________________________

    DAKOTA: Design Analysis Kit for Optimization and Terascale Applications
    Copyright 2014-2022
    National Technology & Engineering Solutions of Sandia, LLC (NTESS).
    This software is distributed under the GNU Lesser General Public License.
    For more information, see the README file in the top Dakota directory.
    _______________________________________________________________________ */

//- Class:	 NonDACVSampling
//- Description: Class for approximate control variate sampling
//- Owner:       Mike Eldred
//- Checked by:
//- Version:

#ifndef NOND_ACV_SAMPLING_H
#define NOND_ACV_SAMPLING_H

#include "NonDNonHierarchSampling.hpp"
//#include "DataMethod.hpp"


namespace Dakota {


/// Perform Approximate Control Variate Monte Carlo sampling for UQ.

/** Approximate Control Variate (ACV) is a variance-reduction technique
    that utilitizes lower fidelity simulations that have response QoI
    that are correlated with the high-fidelity response QoI. */

class NonDACVSampling: public NonDNonHierarchSampling
{
public:

  //
  //- Heading: Constructors and destructor
  //

  /// standard constructor
  NonDACVSampling(ProblemDescDB& problem_db, Model& model);
  /// destructor
  ~NonDACVSampling();

  //
  //- Heading: Virtual function redefinitions
  //

  //bool resize();

protected:

  //
  //- Heading: Virtual function redefinitions
  //

  //void pre_run();
  void core_run();
  //void post_run(std::ostream& s);
  //void print_results(std::ostream& s, short results_state = FINAL_RESULTS);
  //void print_variance_reduction(std::ostream& s);

  //
  //- Heading: member functions
  //

  void approximate_control_variate();
  void approximate_control_variate_offline_pilot();
  void approximate_control_variate_pilot_projection();

  void approx_increments(IntRealMatrixMap& sum_L_baselineH,
			 IntRealVectorMap& sum_H,
			 IntRealSymMatrixArrayMap& sum_LL,
			 IntRealMatrixMap& sum_LH, const SizetArray& N_H_actual,
			 size_t N_H_alloc, const RealVector& avg_eval_ratios,
			 Real avg_hf_target);
  bool acv_approx_increment(const RealVector& avg_eval_ratios,
			    const Sizet2DArray& N_L_actual_refined,
			    SizetArray& N_L_alloc_refined, Real hf_target,
			    size_t iter, const SizetArray& approx_sequence,
			    size_t start, size_t end);

  void compute_ratios(const RealMatrix& var_L,     const RealVector& cost,
		      RealVector& avg_eval_ratios, Real& avg_hf_target,
		      Real& avg_estvar,            Real& avg_estvar_ratio);

private:

  //
  //- Heading: Helper functions
  //

  void initialize_acv_sums(IntRealMatrixMap& sum_L_baseline,
			   IntRealVectorMap& sum_H,
			   IntRealSymMatrixArrayMap& sum_LL,
			   IntRealMatrixMap& sum_LH, RealVector& sum_HH);

  void initialize_acv_counts(SizetArray& num_H, SizetSymMatrixArray& num_LL);

  //void initialize_acv_covariances(IntRealSymMatrixArrayMap covLL,
  //				  IntRealMatrixMap& cov_LH,
  //				  IntRealVectorMap& var_H);

  // shared_increment() cases:
  void accumulate_acv_sums(IntRealMatrixMap& sum_L_baseline,
			   IntRealVectorMap& sum_H,
			   IntRealSymMatrixArrayMap& sum_LL,
			   IntRealMatrixMap& sum_LH, RealVector& sum_HH,
			   SizetArray& N_shared);
  void accumulate_acv_sums(RealMatrix& sum_L_baseline, RealVector& sum_H,
			   RealSymMatrixArray& sum_LL, RealMatrix& sum_LH,
			   RealVector& sum_HH, SizetArray& N_shared);
  // shared_approx_increment() case:
  void accumulate_acv_sums(IntRealMatrixMap& sum_L_baseline,
			   IntRealSymMatrixArrayMap& sum_LL,
			   Sizet2DArray& N_L_shared);
  // approx_increment() cases:
  void accumulate_acv_sums(IntRealMatrixMap& sum_L_refined,
			   Sizet2DArray& N_L_refined,
			   const SizetArray& approx_sequence,
			   size_t approx_start, size_t approx_end);

  void compute_LH_covariance(const RealMatrix& sum_L_shared,
			     const RealVector& sum_H, const RealMatrix& sum_LH,
			     const SizetArray& N_shared, RealMatrix& cov_LH);
  void compute_LL_covariance(const RealMatrix& sum_L_shared,
			     const RealSymMatrixArray& sum_LL,
			     const SizetArray& N_shared,
			     RealSymMatrixArray& cov_LL);
  void covariance_to_correlation_sq(const RealMatrix& cov_LH,
				    const RealMatrix& var_L,
				    const RealVector& var_H,
				    RealMatrix& rho2_LH);

  void compute_L_variance(const RealMatrix& sum_L,
			  const RealSymMatrixArray& sum_LL,
			  const SizetArray& num_L, RealMatrix& var_L);

  void scale_to_target(Real avg_N_H, const RealVector& cost,
		       RealVector& avg_eval_ratios, Real& avg_hf_target);

  Real acv_estimator_variance(const RealVector& avg_eval_ratios,
			      Real avg_hf_target);
  Real acv_estimator_variance(Real avg_N_H, const RealVector& cost,
			      RealVector& avg_eval_ratios, Real& avg_hf_target);

  void acv_raw_moments(IntRealMatrixMap& sum_L_shared,
		       IntRealMatrixMap& sum_L_refined, IntRealVectorMap& sum_H,
		       IntRealSymMatrixArrayMap& sum_LL,
		       IntRealMatrixMap& sum_LH,
		       const RealVector& avg_eval_ratios,
		       const SizetArray& N_shared,
		       const Sizet2DArray& N_L_refined, RealMatrix& H_raw_mom);

  void compute_acv_control(const RealSymMatrix& cov_LL, const RealSymMatrix& F,
			   const RealMatrix& cov_LH, size_t qoi,
			   RealVector& beta);
  void compute_acv_control(RealMatrix& sum_L, Real sum_H_q,
			   RealSymMatrix& sum_LL_q, RealMatrix& sum_LH,
			   size_t N_shared_q, const RealSymMatrix& F,
			   size_t qoi, RealVector& beta);

  void update_projected_lf_samples(Real avg_hf_targets,
				   const RealVector& avg_eval_ratios,
				   const SizetArray& N_H_actual,
				   size_t& N_H_alloc,
				   //SizetArray& delta_N_L_actual,
				   Real& delta_equiv_hf);
  void update_projected_samples(Real avg_hf_targets,
				const RealVector& avg_eval_ratios,
				const SizetArray& N_H_actual, size_t& N_H_alloc,
				size_t& delta_N_H_actual,
				//SizetArray& delta_N_L_actual,
				Real& delta_equiv_hf);

  Real update_hf_target(const RealVector& avg_eval_ratios,
			const RealVector& var_H, const RealVector& estvar0);

  //
  //- Heading: Data
  //

  /// option for performing multiple ACV optimizations and taking the best
  bool multiStartACV;
};


inline void NonDACVSampling::
initialize_acv_sums(IntRealMatrixMap& sum_L_baseline, IntRealVectorMap& sum_H,
		    IntRealSymMatrixArrayMap& sum_LL, IntRealMatrixMap& sum_LH,
		    RealVector& sum_HH)
{
  initialize_sums(sum_L_baseline, sum_H, sum_LH, sum_HH);
  std::pair<int, RealSymMatrixArray> mat_array_pr;
  for (int i=1; i<=4; ++i) {
    mat_array_pr.first = i; // moment number
    RealSymMatrixArray& mat_array = sum_LL.insert(mat_array_pr).first->second;
    mat_array.resize(numFunctions);
    for (size_t j=0; j<numFunctions; ++j)
      mat_array[j].shape(numApprox);
  }
}


inline void NonDACVSampling::
initialize_acv_counts(SizetArray& num_H, SizetSymMatrixArray& num_LL)
{
  num_H.assign(numFunctions, 0);
  num_LL.resize(numFunctions);
  for (size_t qoi=0; qoi<numFunctions; ++qoi)
    num_LL[qoi].shape(numApprox);
}


/*
inline void NonDACVSampling::
initialize_acv_counts(Sizet2DArray& num_L_baseline,  SizetArray& num_H,
		      SizetSymMatrixArray& num_LL, Sizet2DArray& num_LH)
{
  initialize_counts(num_L_baseline, num_H, num_LH);
  num_LL.resize(numFunctions);
  for (size_t qoi=0; qoi<numFunctions; ++qoi)
    num_LL[qoi].shape(numApprox);
}


inline void NonDACVSampling::
initialize_acv_covariances(IntRealSymMatrixArrayMap covLL,
			   IntRealMatrixMap& cov_LH, IntRealVectorMap& var_H)
{
  std::pair<int, RealVector> vec_pr;  std::pair<int, RealMatrix> mat_pr;
  std::pair<int, RealSymMatrixArray> mat_array_pr;
  for (int i=1; i<=4; ++i) {
    vec_pr.first = mat_pr.first = mat_array_pr.first = i; // moment number
    var_H.insert(vec_pr).first->second.size(numFunctions);
    cov_LH.insert(mat_pr).first->second.shape(numFunctions, numApprox);
    RealSymMatrixArray& mat_array = covLL.insert(mat_array_pr).first->second;
    mat_array.resize(numFunctions);
    for (size_t j=0; j<numFunctions; ++j)
      mat_array[i].shape(numApprox);
  }
}
*/


inline void NonDACVSampling::
compute_L_variance(const RealMatrix& sum_L, const RealSymMatrixArray& sum_LL,
		   const SizetArray& num_L, RealMatrix& var_L)
{
  if (var_L.empty()) var_L.shapeUninitialized(numFunctions, numApprox);

  size_t qoi, approx, num_L_q;
  for (qoi=0; qoi<numFunctions; ++qoi) {
    num_L_q = num_L[qoi];
    for (approx=0; approx<numApprox; ++approx)
      compute_variance(sum_L(qoi,approx), sum_LL[qoi](approx,approx),
		       num_L_q, var_L(qoi,approx));
  }
}


inline void NonDACVSampling::
covariance_to_correlation_sq(const RealMatrix& cov_LH, const RealMatrix& var_L,
			     const RealVector& var_H, RealMatrix& rho2_LH)
{
  if (rho2_LH.empty()) rho2_LH.shapeUninitialized(numFunctions, numApprox);

  size_t qoi, approx;  Real var_H_q, cov_LH_aq;
  for (qoi=0; qoi<numFunctions; ++qoi) {
    var_H_q = var_H[qoi];
    for (approx=0; approx<numApprox; ++approx) {
      cov_LH_aq = cov_LH(qoi,approx);
      rho2_LH(qoi,approx) = cov_LH_aq / var_L(qoi,approx) * cov_LH_aq / var_H_q;
    }
  }
}


inline void NonDACVSampling::
scale_to_target(Real avg_N_H, const RealVector& cost,
		RealVector& avg_eval_ratios, Real& avg_hf_target)
{
  // scale to enforce budget constraint.  Since the profile does not emerge
  // from pilot in ACV, don't select an infeasible initial guess:
  // > if N* < N_pilot, scale back r* --> initial = scaled_r*,N_pilot
  // > if N* > N_pilot, use initial = r*,N*
  avg_hf_target = allocate_budget(avg_eval_ratios, cost); // r* --> N*
  if (avg_N_H > avg_hf_target) {// replace N* with N_pilot, rescale r* to budget
    avg_hf_target = avg_N_H;
    scale_to_budget_with_pilot(avg_eval_ratios, cost, avg_hf_target);
  }
}


inline Real NonDACVSampling::
acv_estimator_variance(const RealVector& avg_eval_ratios, Real avg_hf_target)
{
  RealSymMatrix F;  RealVector estvar_ratios, estvar(numFunctions, false);
  compute_F_matrix(avg_eval_ratios, F);
  acv_estvar_ratios(F, estvar_ratios);
  for (size_t qoi=0; qoi<numFunctions; ++qoi)
    estvar[qoi] = varH[qoi] / avg_hf_target * estvar_ratios[qoi];
  // Note: matrix ops performed for each QoI and then averaged.
  //       While r_i is averaged such that there is only one F,
  //       we use per-QoI covLL (C matrix) and covLH (c vector).
  return average(estvar);
}


inline Real NonDACVSampling::
acv_estimator_variance(Real avg_N_H, const RealVector& cost,
		       RealVector& avg_eval_ratios, Real& avg_hf_target)
{
  scale_to_target(avg_N_H, cost, avg_eval_ratios, avg_hf_target);
  return acv_estimator_variance(avg_eval_ratios, avg_hf_target);
}


inline void NonDACVSampling::
compute_acv_control(const RealSymMatrix& cov_LL, const RealSymMatrix& F,
		    const RealMatrix& cov_LH, size_t qoi, RealVector& beta)
{
  RealSymMatrix CF_inv;  RealVector A;
  invert_CF(cov_LL, F, CF_inv);
  //Cout << "compute_acv_control qoi " << qoi+1 << ": CF_inv\n" << CF_inv;
  compute_A_vector(F, cov_LH, qoi, A); // no scaling (uses c, not c-bar)
  //Cout << "compute_acv_control qoi " << qoi+1 << ": A\n" << A;

  size_t n = F.numRows();
  if (beta.length() != n) beta.size(n);
  beta.multiply(Teuchos::LEFT_SIDE, 1., CF_inv, A, 0.); // for SymMatrix mult
  //Cout << "compute_acv_control qoi " << qoi+1 << ": beta\n" << beta;
}


inline void NonDACVSampling::
compute_acv_control(RealMatrix& sum_L, Real sum_H_q, RealSymMatrix& sum_LL_q,
		    RealMatrix& sum_LH, size_t N_shared_q,
		    const RealSymMatrix& F, size_t qoi, RealVector& beta)
{
  // compute cov_LL, cov_LH, var_H across numApprox for a particular QoI
  // > cov_LH is sized for all qoi but only 1 row is used
  size_t approx, approx2;  Real sum_L_aq;
  RealSymMatrix cov_LL(numApprox); RealMatrix cov_LH(numFunctions, numApprox);

  for (approx=0; approx<numApprox; ++approx) {
    sum_L_aq = sum_L(qoi,approx);
    compute_covariance(sum_L_aq, sum_H_q, sum_LH(qoi,approx), N_shared_q,
		       cov_LH(qoi,approx));
    compute_variance(sum_L_aq, sum_LL_q(approx,approx), N_shared_q,
		     cov_LL(approx,approx));
    for (approx2=0; approx2<approx; ++approx2)
      compute_covariance(sum_L_aq, sum_L(qoi,approx2), sum_LL_q(approx,approx2),
			 N_shared_q, cov_LL(approx,approx2));
  }

  // forward to overload:
  compute_acv_control(cov_LL, F, cov_LH, qoi, beta);
}


/*
inline void NonDACVSampling::
compute_acv_control(RealMatrix& sum_L, Real sum_H_q, RealSymMatrix& sum_LL_q,
		    RealMatrix& sum_LH, const Sizet2DArray& num_L,
		    size_t num_H_q, const SizetSymMatrix& num_LL_q,
		    const Sizet2DArray& num_LH, const RealSymMatrix& F,
		    size_t qoi, RealVector& beta)
{
  // compute cov_LL, cov_LH, var_H across numApprox for a particular QoI
  // > cov_LH is sized for all qoi but only 1 row is used
  size_t approx, approx2, num_L_aq;  Real sum_L_aq;
  RealSymMatrix cov_LL(numApprox); RealMatrix cov_LH(numFunctions, numApprox);

  for (approx=0; approx<numApprox; ++approx) {
    num_L_aq = num_L[approx][qoi];  sum_L_aq = sum_L(qoi,approx);
    compute_covariance(sum_L_aq, sum_H_q, sum_LH(qoi,approx), num_L_aq,
		       num_H_q, num_LH[approx][qoi], cov_LH(qoi,approx));
    compute_variance(sum_L_aq, sum_LL_q(approx,approx), num_L_aq,
		     cov_LL(approx,approx));
    for (approx2=0; approx2<approx; ++approx2)
      compute_covariance(sum_L_aq, sum_L(qoi,approx2), sum_LL_q(approx,approx2),
			 num_L_aq, num_L[approx2][qoi],num_LL_q(approx,approx2),
			 cov_LL(approx,approx2));
  }

  // forward to overload:
  compute_acv_control(cov_LL, F, cov_LH, qoi, beta);
}
*/

} // namespace Dakota

#endif
