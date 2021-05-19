/*  _______________________________________________________________________

    DAKOTA: Design Analysis Kit for Optimization and Terascale Applications
    Copyright 2014-2020
    National Technology & Engineering Solutions of Sandia, LLC (NTESS).
    This software is distributed under the GNU Lesser General Public License.
    For more information, see the README file in the top Dakota directory.
    _______________________________________________________________________ */

//- Class:	 NonDMultifidelitySampling
//- Description: Implementation code for NonDMultifidelitySampling class
//- Owner:       Mike Eldred
//- Checked by:
//- Version:

#include "dakota_system_defs.hpp"
#include "dakota_data_io.hpp"
#include "dakota_tabular_io.hpp"
#include "DakotaModel.hpp"
#include "DakotaResponse.hpp"
#include "NonDMultifidelitySampling.hpp"
#include "ProblemDescDB.hpp"
#include "ActiveKey.hpp"
#include "DakotaIterator.hpp"

static const char rcsId[]="@(#) $Id: NonDMultifidelitySampling.cpp 7035 2010-10-22 21:45:39Z mseldre $";


namespace Dakota {

/** This constructor is called for a standard letter-envelope iterator 
    instantiation.  In this case, set_db_list_nodes has been called and 
    probDescDB can be queried for settings from the method specification. */
NonDMultifidelitySampling::
NonDMultifidelitySampling(ProblemDescDB& problem_db, Model& model):
  NonDHierarchSampling(problem_db, model), finalCVRefinement(true)
{
  // For now...
  size_t num_mf = NLev.size();
  if (num_mf > 2) {
    Cerr << "Warning: NonDMultifidelitySampling currently uses first and last "
	 << "model in ordered sequence and ignores the rest." << std::endl;
    //abort_handler(METHOD_ERROR);
  }
}


/** The primary run function manages the general case: a hierarchy of model 
    forms (from the ordered model fidelities within a HierarchSurrModel), 
    each of which may contain multiple discretization levels. */
void NonDMultifidelitySampling::core_run()
{
  // remove default key (empty activeKey) since this interferes with approx
  // combination in MF surrogates.  Also useful for ML/MF re-entrancy.
  iteratedModel.clear_model_keys();
  // prefer ML over MF if both available
  iteratedModel.multifidelity_precedence(true);

  // May retire control_variate_mc() in time, but retain for right now...
  //size_t num_steps, fixed_index;  short seq_type;
  //configure_sequence(num_steps, fixed_index, seq_type);
  //if (num_steps == 2)
  control_variate_mc();   // Ng and Willcox, 2014
  //else
  //  multifidelity_mc(); // Peherstorfer, Willcox, Gunzburger, 2016

  // Note: MFMC uses a nested sampling pattern which does not mesh with the
  // model pairing assumed in HierarchSurrModel --> either support MFMC under
  // NonDEnsembleSampling or subsume HierarchSurrModel as a special case of
  // NonHierarchSurrModel (in which case, SurrogateModel alignment becomes
  // unimportant).
}


/** This function performs control variate MC across two combinations of 
    model form and discretization level. */
void NonDMultifidelitySampling::control_variate_mc()
{
  // Performs pilot + LF increment and then iterates with additional shared
  // increment + LF increment batches until prescribed MSE reduction is obtained

  size_t qoi, num_steps, form, lev, fixed_index;  short seq_type;
  configure_sequence(num_steps, fixed_index, seq_type);
  bool multilev = (seq_type == Pecos::RESOLUTION_LEVEL_SEQUENCE);
  // For two-model control variate, select extreme fidelities/resolutions
  Pecos::ActiveKey active_key, hf_key, lf_key;
  unsigned short hf_form, lf_form;
  size_t hf_lev, lf_lev, SZ_MAX = std::numeric_limits<size_t>::max();
  if (multilev) {
    hf_form = lf_form = (fixed_index == SZ_MAX) ? USHRT_MAX : fixed_index;
    hf_lev  = num_steps-1;  lf_lev = 0;  // extremes of range
  }
  else {
    hf_form = num_steps-1;  lf_form = 0; // extremes of range
    hf_lev = lf_lev = fixed_index;
  }
  hf_key.form_key(0, hf_form, hf_lev);
  lf_key.form_key(0, lf_form, lf_lev);
  active_key.aggregate_keys(hf_key, lf_key, Pecos::RAW_DATA);

  aggregated_models_mode();
  iteratedModel.active_model_key(active_key); // data group 0

  // retrieve cost estimates across model forms for a particular soln level
  Real lf_cost, hf_cost;
  size_t hf_form_index, lf_form_index, hf_lev_index, lf_lev_index;
  if (multilev) {
    RealVector cost;  configure_cost(num_steps, multilev, cost);
    hf_cost = cost[hf_lev];  hf_lev_index = hf_lev;
    lf_cost = cost[lf_lev];  lf_lev_index = lf_lev;
    hf_form_index = (hf_form == USHRT_MAX) ? 0 : hf_form; // should not happen
    lf_form_index = (lf_form == USHRT_MAX) ? 0 : lf_form; // should not happen
  }
  else {
    Model& truth_model = iteratedModel.truth_model();
    Model&  surr_model = iteratedModel.surrogate_model();
    hf_cost       = truth_model.solution_level_cost();          // active
    lf_cost       =  surr_model.solution_level_cost();          // active
    hf_form_index = hf_form;  lf_form_index = lf_form;
    size_t raw_index = truth_model.solution_level_cost_index(); // active
    hf_lev_index  = (raw_index == SZ_MAX) ? 0 : raw_index;
    raw_index     =  surr_model.solution_level_cost_index();    // active
    lf_lev_index  = (raw_index == SZ_MAX) ? 0 : raw_index;
  }
  Real cost_ratio = hf_cost / lf_cost;
  SizetArray& N_hf = NLev[hf_form_index][hf_lev_index];
  SizetArray& N_lf = NLev[lf_form_index][lf_lev_index];
  N_hf.assign(numFunctions, 0);  N_lf.assign(numFunctions, 0);
  size_t raw_N_hf = 0, raw_N_lf = 0;

  IntRealVectorMap sum_L_shared, sum_L_refined, sum_H, sum_LL, sum_LH;
  initialize_mf_sums(sum_L_shared, sum_L_refined, sum_H, sum_LL, sum_LH);
  RealVector sum_HH(numFunctions), var_H(numFunctions, false),
    rho2_LH(numFunctions, false), mu_hat, eval_ratios, mse_ratios;

  // Initialize for pilot sample
  SizetArray delta_N_l;
  load_pilot_sample(pilotSamples, 2, delta_N_l); // 2 models only for now
  size_t hf_sample_incr = std::min(delta_N_l[lf_form], delta_N_l[hf_form]);
  numSamples = hf_sample_incr;

  mlmfIter = 0;  equivHFEvals = 0.;
  size_t max_iter = (maxIterations < 0) ? 25 : maxIterations; // default = -1
  while (hf_sample_incr  &&  mlmfIter <= max_iter  &&
	 equivHFEvals <= maxFunctionEvals) {

    // ----------------------------------------------------------
    // Compute shared increment targeting specified MSE reduction
    // ----------------------------------------------------------
    if (mlmfIter) {
      // CV MSE target = convTol * mcMSEIter0 = mse_ratio * var_H / N_hf
      // N_hf = mse_ratio * var_H / convTol / mcMSEIter0
      // Note: don't simplify further since mcMSEIter0 is based on pilot
      //       estimate of var_H / N_hf
      RealVector hf_targets = mse_ratios;
      for (qoi=0; qoi<numFunctions; ++qoi)
	hf_targets[qoi] *= var_H[qoi] / mcMSEIter0[qoi] / convergenceTol;
      // Power mean choice: average, max (desire would be to balance overshoot
      // vs. additional iteration)
      hf_sample_incr = numSamples = one_sided_delta(N_hf, hf_targets, 1); //avg
    }

    if (numSamples) {
      shared_increment(active_key, mlmfIter, 0);
      accumulate_mf_sums(sum_L_shared, sum_L_refined, sum_H, sum_LL, sum_LH,
			 sum_HH, mu_hat, N_lf, N_hf);
      raw_N_lf += numSamples; raw_N_hf += numSamples;
      // Compute the LF/HF evaluation ratio using shared samples, averaged
      // over QoI.  This includes updating var_H and rho2_LH.
      compute_eval_ratios(sum_L_shared[1], sum_H[1], sum_LL[1], sum_LH[1],
			  sum_HH, cost_ratio, N_hf, var_H, rho2_LH,
			  eval_ratios);
      // Compute the ratio of MC and CVMC mean squared errors (for convergence).
      // This ratio incorporates the anticipated variance reduction from the
      // upcoming application of eval_ratios.
      compute_MSE_ratios(eval_ratios, var_H, rho2_LH, mlmfIter, N_hf,
			 mse_ratios);

      // -------------------------------------------------------------------
      // Compute new LF increment based on new evaluation ratio for new N_hf
      // -------------------------------------------------------------------
      // *** TO DO: how to allow user to stop after pilot only
      // *** Note: lf_increment includes finalCVRefinement flag (=true)
      // *** Consider maxFunctionEvals throttle (see also compute_eval_ratios)
      if (lf_increment(lf_key, eval_ratios, N_lf, N_hf, mlmfIter, 0)) {
	accumulate_mf_sums(sum_L_refined, mu_hat, N_lf);
	raw_N_lf += numSamples; //lf_sample_incr = numSamples;
      }
    }
    //Cout << "\nCVMC iteration " << mlmfIter << " complete." << std::endl;
    ++mlmfIter;
    // update equivalent number of HF evaluations (no credit for failed evals)
    equivHFEvals = raw_N_hf + (Real)raw_N_lf / cost_ratio;
  } // end while

  // Compute/apply control variate parameter to estimate uncentered raw moments
  RealMatrix H_raw_mom(numFunctions, 4);
  cv_raw_moments(sum_L_shared, sum_H, sum_LL, sum_LH, N_hf, sum_L_refined, N_lf,
		 rho2_LH, H_raw_mom);
  // Convert uncentered raw moment estimates to final moments (central or std)
  convert_moments(H_raw_mom, momentStats);
}


/** This function performs control variate MC across two combinations of 
    model form and discretization level.
void NonDMultifidelitySampling::multifidelity_mc()
{
  // Performs pilot + LF increment and then iterates with additional shared
  // increment + LF increment batches until prescribed MSE reduction is obtained

  size_t qoi, num_steps, form, lev, fixed_index;  short seq_type;
  configure_sequence(num_steps, fixed_index, seq_type);
  bool multilev = (seq_type == Pecos::RESOLUTION_LEVEL_SEQUENCE);

  // 1D sequence: either lev varies and form is fixed, or vice versa:
  size_t& step = (multilev) ? lev : form;
  if (multilev) form = secondary_index;
  else          lev  = secondary_index;

  size_t max_iter = (maxIterations < 0) ? 25 : maxIterations; // default = -1
  // retrieve cost estimates across soln levels for a particular model form
  RealVector cost; configure_cost(num_steps, multilev, cost);

  // Initialize for pilot sample
  SizetArray delta_N_l;
  load_pilot_sample(pilotSamples, num_steps, delta_N_l);
  size_t hf_sample_incr = delta_N_l[hf_form];
  numSamples = hf_sample_incr;

  mlmfIter = 0;  equivHFEvals = 0.;
  size_t max_iter = (maxIterations < 0) ? 25 : maxIterations; // default = -1
  while (hf_sample_incr  &&  mlmfIter <= max_iter  &&
	 equivHFEvals <= maxFunctionEvals) {

   shared_increment(); // spans models {i, i+1, ..., M}
   // to support with HierarchSurrModel, might be simplest to sample one model 
   // at a time using a nested sample set...

    ++mlmfIter;
    // compute the equivalent number of HF evaluations
    compute_equivalent_cost(raw_N_l, cost);
  } // end while

  // Compute/apply control variate parameter to estimate uncentered raw moments
  RealMatrix H_raw_mom(numFunctions, 4);
  cv_raw_moments(sum_L_shared, sum_H, sum_LL, sum_LH, N_hf, sum_L_refined, N_lf,
		 rho2_LH, H_raw_mom);
  // Convert uncentered raw moment estimates to final moments (central or std)
  convert_moments(H_raw_mom, momentStats);
}
*/


void NonDMultifidelitySampling::
initialize_mf_sums(IntRealVectorMap& sum_L_shared,
		   IntRealVectorMap& sum_L_refined, IntRealVectorMap& sum_H,
		   IntRealVectorMap& sum_LL,      //IntRealVectorMap& sum_HH,
		   IntRealVectorMap& sum_LH)
{
  // sum_* are running sums across all increments
  std::pair<int, RealVector> empty_pr;
  for (int i=1; i<=4; ++i) {
    empty_pr.first = i;
    // std::map::insert() returns std::pair<IntRVMIter, bool>:
    // use iterator to size RealVector in place and init sums to 0
    sum_L_shared.insert(empty_pr).first->second.size(numFunctions);
    sum_L_refined.insert(empty_pr).first->second.size(numFunctions);
    sum_H.insert(empty_pr).first->second.size(numFunctions);
    sum_LL.insert(empty_pr).first->second.size(numFunctions);
  //sum_HH.insert(empty_pr).first->second.size(numFunctions);
    sum_LH.insert(empty_pr).first->second.size(numFunctions);
  }
}


void NonDMultifidelitySampling::
accumulate_mf_sums(IntRealVectorMap& sum_L, const RealVector& offset,
		   SizetArray& num_L)
{
  // uses one set of allResponses in BYPASS_SURROGATE mode
  // IntRealVectorMap is not a multilevel case --> no discrepancies

  using std::isfinite;
  Real fn_val, prod;
  int ord, active_ord; size_t qoi;
  IntRespMCIter r_it; IntRVMIter l_it;
  bool os = !offset.empty();

  for (r_it=allResponses.begin(); r_it!=allResponses.end(); ++r_it) {
    const RealVector& fn_vals = r_it->second.function_values();

    for (qoi=0; qoi<numFunctions; ++qoi) {
      prod = fn_val = (os) ? fn_vals[qoi] - offset[qoi] : fn_vals[qoi];

      if (isfinite(fn_val)) { // neither NaN nor +/-Inf
	l_it = sum_L.begin(); ord = l_it->first; active_ord = 1;
	while (l_it!=sum_L.end()) {
    
	  if (ord == active_ord) {
	    l_it->second[qoi] += prod; ++l_it;
	    ord = (l_it == sum_L.end()) ? 0 : l_it->first;
	  }

	  prod *= fn_val; ++active_ord;
	}
	++num_L[qoi];
      }
    }
  }
}


void NonDMultifidelitySampling::
accumulate_mf_sums(IntRealVectorMap& sum_L_shared,
		   IntRealVectorMap& sum_L_refined, IntRealVectorMap& sum_H,
		   IntRealVectorMap& sum_LL, IntRealVectorMap& sum_LH,
		   RealVector& sum_HH, const RealVector& offset,
		   SizetArray& num_L, SizetArray& num_H)
{
  // uses one set of allResponses in AGGREGATED_MODELS mode
  // IntRealVectorMap is not a multilevel case so no discrepancies

  using std::isfinite;
  Real lf_fn, hf_fn, lf_prod, hf_prod;
  IntRespMCIter r_it; IntRVMIter ls_it, lr_it, h_it, ll_it, lh_it;
  int ls_ord, lr_ord, h_ord, ll_ord, lh_ord, active_ord; size_t qoi;
  bool os = !offset.empty();

  for (r_it=allResponses.begin(); r_it!=allResponses.end(); ++r_it) {
    const RealVector& fn_vals = r_it->second.function_values();

    for (qoi=0; qoi<numFunctions; ++qoi) {

      // response mode AGGREGATED_MODELS orders HF (active model key)
      // followed by LF (previous/decremented model key)
      hf_prod = hf_fn = (os) ? fn_vals[qoi] - offset[qoi] : fn_vals[qoi];
      lf_prod = lf_fn = (os) ?
	fn_vals[qoi+numFunctions] - offset[qoi+numFunctions] :
	fn_vals[qoi+numFunctions];

      // sync sample counts for all L and H interactions at this level
      if (isfinite(lf_fn) && isfinite(hf_fn)) { // neither NaN nor +/-Inf

	// High-High
	sum_HH[qoi] += hf_prod * hf_prod;

	ls_it = sum_L_shared.begin(); lr_it = sum_L_refined.begin();
	h_it  = sum_H.begin(); ll_it = sum_LL.begin(); lh_it = sum_LH.begin();
	ls_ord = /*(ls_it == sum_L_shared.end())  ? 0 :*/ ls_it->first;
	lr_ord = /*(lr_it == sum_L_refined.end()) ? 0 :*/ lr_it->first;
	h_ord  = /*(h_it  == sum_H.end())  ? 0 :*/  h_it->first;
	ll_ord = /*(ll_it == sum_LL.end()) ? 0 :*/ ll_it->first;
	lh_ord = /*(lh_it == sum_LH.end()) ? 0 :*/ lh_it->first;
	active_ord = 1;
	while (ls_it!=sum_L_shared.end() || lr_it!=sum_L_refined.end() ||
	       h_it!=sum_H.end() || ll_it!=sum_LL.end() ||
	       lh_it!=sum_LH.end() || active_ord <= 1) {
    
	  // Low shared
	  if (ls_ord == active_ord) {
	    ls_it->second[qoi] += lf_prod;
	    ++ls_it; ls_ord = (ls_it == sum_L_shared.end())  ? 0 : ls_it->first;
	  }
	  // Low refined
	  if (lr_ord == active_ord) {
	    lr_it->second[qoi] += lf_prod;
	    ++lr_it; lr_ord = (lr_it == sum_L_refined.end()) ? 0 : lr_it->first;
	  }
	  // High
	  if (h_ord == active_ord) {
	    h_it->second[qoi] += hf_prod;
	    ++h_it; h_ord = (h_it == sum_H.end()) ? 0 : h_it->first;
	  }
	  // Low-Low
	  if (ll_ord == active_ord) {
	    ll_it->second[qoi] += lf_prod * lf_prod;
	    ++ll_it; ll_ord = (ll_it == sum_LL.end()) ? 0 : ll_it->first;
	  }
	  // Low-High
	  if (lh_ord == active_ord) {
	    lh_it->second[qoi] += lf_prod * hf_prod;
	    ++lh_it; lh_ord = (lh_it == sum_LH.end()) ? 0 : lh_it->first;
	  }

	  if (ls_ord || lr_ord || ll_ord || lh_ord) lf_prod *= lf_fn;
	  if (h_ord  || lh_ord)                     hf_prod *= hf_fn;
	  ++active_ord;
	}
	++num_L[qoi]; ++num_H[qoi];
      }
    }
  }
}


void NonDMultifidelitySampling::
shared_increment(const Pecos::ActiveKey& agg_key, size_t iter, size_t lev)
{
  if (iter == _NPOS)  Cout << "\nCVMC sample increments: ";
  else if (iter == 0) Cout << "\nCVMC pilot sample: ";
  else Cout << "\nCVMC iteration " << iter << " sample increments: ";
  Cout << "LF = " << numSamples << " HF = " << numSamples << '\n';

  if (numSamples) {
    aggregated_models_mode();
    iteratedModel.active_model_key(agg_key);

    // generate new MC parameter sets
    get_parameter_sets(iteratedModel);// pull dist params from any model

    // export separate output files for each data set:
    if (exportSampleSets) // for HF+LF models, use the HF tags
      export_all_samples("cv_", iteratedModel.truth_model(), iter, lev);

    // compute allResponses from allVariables using hierarchical model
    evaluate_parameter_sets(iteratedModel, true, false);
  }
}


/** version with LF key */
bool NonDMultifidelitySampling::
lf_increment(const Pecos::ActiveKey& lf_key, const RealVector& eval_ratios,
	     const SizetArray& N_lf, const SizetArray& N_hf,
	     size_t iter, size_t lev)
{
  lf_increment_samples(eval_ratios, N_lf, N_hf);
  if (numSamples) {
    uncorrected_surrogate_mode(); // also needed for lf_key assignment below
    iteratedModel.active_model_key(lf_key); // sets activeKey and surrModelKey

    return lf_increment(iter, lev);
  }
  else
    return false;
}


/** version without LF key */
bool NonDMultifidelitySampling::
lf_increment(const RealVector& eval_ratios, const SizetArray& N_lf,
	     const SizetArray& N_hf, size_t iter, size_t lev)
{
  lf_increment_samples(eval_ratios, N_lf, N_hf);
  return (numSamples) ? lf_increment(iter, lev) : false;
}


/** shared helper */
void NonDMultifidelitySampling::
lf_increment_samples(const RealVector& eval_ratios, const SizetArray& N_lf,
		     const SizetArray& N_hf)
{
  // update LF samples based on evaluation ratio
  //   r = m/n -> m = r*n -> delta = m-n = (r-1)*n
  //   or with inverse r  -> delta = m-n = n/inverse_r - n
  RealVector lf_targets(numFunctions, false);
  for (size_t qoi=0; qoi<numFunctions; ++qoi)
    lf_targets[qoi] = eval_ratios[qoi] * N_hf[qoi];
  // Choose average, RMS, max of difference?
  // Trade-off: Possible overshoot vs. more iteration...
  numSamples = one_sided_delta(N_lf, lf_targets, 1); // average

  if (numSamples) Cout << "\nCVMC LF sample increment = " << numSamples;
  else            Cout << "\nNo CVMC LF sample increment";
  if (outputLevel >= DEBUG_OUTPUT)
    Cout << " from avg LF = " << average(N_lf) << ", avg HF = "
	 << average(N_hf) << ", avg eval_ratio = " << average(eval_ratios);
  Cout << std::endl;
}


/** shared helper */
bool NonDMultifidelitySampling::lf_increment(size_t iter, size_t lev)
{
  // ----------------------------------------
  // Compute LF increment for control variate
  // ----------------------------------------

  // generate new MC parameter sets
  get_parameter_sets(iteratedModel);// pull dist params from any model
  // export separate output files for each data set:
  if (exportSampleSets)
    export_all_samples("cv_", iteratedModel.surrogate_model(), iter, lev);

  size_t max_iter = (maxIterations < 0) ? 25 : maxIterations; // default = -1
  // Iteration 0 is defined as the pilot sample + initial CV increment, and
  // each subsequent iter can be defined as a pair of ML + CV increments.  If
  // it is desired to stop between the ML and CV components, finalCVRefinement
  // can be hardwired to false (not currently part of input spec).
  // Note: termination based on delta_N_hf=0 has final ML and CV increments
  //       of zero, which is consistent with finalCVRefinement=true.
  if (iter < max_iter || finalCVRefinement) {
    // compute allResponses from allVariables using hierarchical model
    evaluate_parameter_sets(iteratedModel, true, false);
    return true;
  }
  else
    return false;
}


void NonDMultifidelitySampling::
compute_eval_ratios(const RealVector& sum_L_shared, const RealVector& sum_H,
		    const RealVector& sum_LL, const RealVector& sum_LH,
		    const RealVector& sum_HH, Real cost_ratio,
		    const SizetArray& N_shared, RealVector& var_H,
		    RealVector& rho2_LH, RealVector& eval_ratios)
{
  if (eval_ratios.empty()) eval_ratios.sizeUninitialized(numFunctions);

  //Real eval_ratio, avg_eval_ratio = 0.; size_t num_avg = 0;
  for (size_t qoi=0; qoi<numFunctions; ++qoi) {

    Real& rho_sq = rho2_LH[qoi];
    compute_mf_correlation(sum_L_shared[qoi], sum_H[qoi], sum_LL[qoi],
			   sum_LH[qoi], sum_HH[qoi], N_shared[qoi],
			   var_H[qoi], rho_sq);

    // compute evaluation ratio which determines increment for LF samples
    // > the sample increment optimizes the total computational budget and is
    //   not treated as a worst case accuracy reqmt --> use the QoI average
    // > refinement based only on QoI mean statistics
    // Given use of 1/r in MSE_ratio, one approach would average 1/r, but
    // this does not seem to behave as well in limited numerical experience.
    //if (rho_sq > Pecos::SMALL_NUMBER) {
    //  avg_inv_eval_ratio += std::sqrt((1. - rho_sq)/(cost_ratio * rho_sq));
    if (rho_sq < 1.) { // protect against division by 0, sqrt(negative)
      eval_ratios[qoi] = std::sqrt(cost_ratio * rho_sq / (1. - rho_sq));
      if (outputLevel >= DEBUG_OUTPUT)
	Cout << "evaluation_ratios() QoI " << qoi+1 << ": cost_ratio = "
	     << cost_ratio << " rho_sq = " << rho_sq << " eval_ratio = "
	     << eval_ratios[qoi] << std::endl;
      //avg_eval_ratio += eval_ratios[qoi];
      //++num_avg;
    }
    else // should not happen, but provide a reasonable upper bound
      eval_ratios[qoi] = (Real)maxFunctionEvals / average(N_shared);
  }
  if (outputLevel >= DEBUG_OUTPUT)
    Cout << "variance of HF Q:\n" << var_H;

  //if (num_avg) avg_eval_ratio /= num_avg;
  //else         avg_eval_ratio  = (Real)maxFunctionEvals / average(N_shared);
  //return avg_eval_ratio;
}


void NonDMultifidelitySampling::
compute_MSE_ratios(const RealVector& eval_ratios, const RealVector& var_H,
		   const RealVector& rho2_LH, size_t iter,
		   const SizetArray& N_hf, RealVector& mse_ratios)
{
  size_t qoi;
  if (iter == 0) {
    // Define mcMSEIter0 for use as a fixed reference (MC variance from pilot
    // sample) for comparing against convergenceTol
    mcMSEIter0.sizeUninitialized(numFunctions);
    for (qoi=0; qoi<numFunctions; ++qoi)
      mcMSEIter0[qoi] = var_H[qoi] / N_hf[qoi];
  }

  //Real curr_mc_mse, curr_cvmc_mse, curr_mse_ratio, avg_mse_ratio = 0.;
  if (mse_ratios.empty()) mse_ratios.sizeUninitialized(numFunctions);
  for (qoi=0; qoi<numFunctions; ++qoi) {
    // Compute ratio of MSE for high fidelity MC and multifidelity CVMC
    // > Estimator Var for MC = sigma_hf^2 / N_hf = MSE (neglect HF bias)
    // > Estimator Var for CV = (1+r/w) [1-rho^2(1-1/r)] sigma_hf^2 / p
    //   where p = (1+r/w)N_hf -> Var = [1-rho^2(1-1/r)] sigma_hf^2 / N_hf
    // MSE ratio = Var_CV / Var_MC = [1-rho^2(1-1/r)]
    mse_ratios[qoi] = 1. - rho2_LH[qoi] * (1. - 1. / eval_ratios[qoi]);//Ng 2014
    Cout << "QoI " << qoi+1 << ": CV variance reduction factor = "
	 << mse_ratios[qoi] << " for eval ratio " << eval_ratios[qoi] << '\n';
    //curr_mc_mse   = var_H[qoi] / N_hf[qoi];
    //curr_cvmc_mse = curr_mc_mse * curr_mse_ratio;
    //Cout << "QoI " << qoi+1 << ": Mean square error estimated to reduce from "
    //     << curr_mc_mse << " (MC) to " << curr_cvmc_mse
    //     << " (CV) following upcoming LF increment\n";

    //if (iter == 0) // initialize reference based on pilot
    //  { mcMSEIter0[qoi] = curr_mc_mse; avg_mse_ratio += curr_mse_ratio; }
    //else           // measure convergence to target based on pilot reference
    //  avg_mse_ratio += curr_cvmc_mse / mcMSEIter0[qoi];
  }

  /*
  avg_mse_ratio /= numFunctions;
  Cout << "Average MSE reduction factor since pilot MC = " << avg_mse_ratio
       << " targeting convergence tol = " << convergenceTol << '\n';
  return avg_mse_ratio;
  */
}


void NonDMultifidelitySampling::
cv_raw_moments(IntRealVectorMap& sum_L_shared, IntRealVectorMap& sum_H,
	       IntRealVectorMap& sum_LL,       IntRealVectorMap& sum_LH,
	       const SizetArray& N_shared,     IntRealVectorMap& sum_L_refined,
	       const SizetArray& N_refined,    const RealVector& rho2_LH,
	       RealMatrix& H_raw_mom)
{
  if (H_raw_mom.empty()) H_raw_mom.shapeUninitialized(numFunctions, 4);
  RealVector beta(numFunctions, false);

  // rho2_LH not stored for i > 1
  for (size_t qoi=0; qoi<numFunctions; ++qoi)
    Cout << "rho_LH (Pearson correlation) for QoI " << qoi+1 << " = "
	 << std::setw(9) << std::sqrt(rho2_LH[qoi]) << '\n';
       //<< ", effectiveness ratio = " << std::setw(9) << rho2_LH[qoi] * cr1;

  for (int i=1; i<=4; ++i) {
    compute_mf_control(sum_L_shared[i], sum_H[i], sum_LL[i], sum_LH[i],
		       N_shared, beta);
    Cout << "Moment " << i << ":\n";
    RealVector H_rm_col(Teuchos::View, H_raw_mom[i-1], numFunctions);
    apply_mf_control(sum_H[i], sum_L_shared[i], N_shared, sum_L_refined[i],
		     N_refined, beta, H_rm_col);
  }
}

} // namespace Dakota
