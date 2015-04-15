/*  _______________________________________________________________________

    DAKOTA: Design Analysis Kit for Optimization and Terascale Applications
    Copyright 2014 Sandia Corporation.
    This software is distributed under the GNU Lesser General Public License.
    For more information, see the README file in the top Dakota directory.
    _______________________________________________________________________ */

//- Class:	 NonDQUESOBayesCalibration
//- Description: Derived class for Bayesian inference using QUESO
//- Owner:       Laura Swiler, Brian Adams
//- Checked by:
//- Version:

// place Dakota headers first to minimize influence of QUESO defines
#include "NonDQUESOBayesCalibration.hpp"
#include "ProblemDescDB.hpp"
#include "ParallelLibrary.hpp"
#include "DakotaModel.hpp"
#include "DakotaApproximation.hpp"
#include "ProbabilityTransformation.hpp"
// then list QUESO headers
#include "queso/StatisticalInverseProblem.h"
#include "queso/StatisticalInverseProblemOptions.h"
#include "queso/MetropolisHastingsSGOptions.h"
#include "queso/SequenceStatisticalOptions.h"
#include "queso/GslVector.h"
#include "queso/GslMatrix.h"
#include "queso/Environment.h"
#include "queso/EnvironmentOptions.h"
#include "queso/Defines.h"
#include "queso/ValidationCycle.h"
#include "queso/GenericScalarFunction.h"
#include "queso/UniformVectorRV.h"

// for generic log prior eval
#include "queso/JointPdf.h"
#include "queso/VectorRV.h"

static const char rcsId[]="@(#) $Id$";


namespace Dakota {

// declaring inherited classes here for now during prototyping to
// avoid including QUESO header in our header

/// Dakota specialization of QUESO generic joint PDF
template <class V, class M>
class QuesoJointPdf: public QUESO::BaseJointPdf<V, M>
{
public:
  //! Default constructor.
  /*! Instantiates an object of the class, i.e. a scalar function,
      given a prefix and its domain.*/
  QuesoJointPdf(const char*                  prefix,
		const QUESO::VectorSet<V,M>& domainSet,
		NonDQUESOBayesCalibration*   nond_queso_ptr);
  //! Destructor
  virtual ~QuesoJointPdf();

  //! Actual value of the PDF (scalar function).
  double actualValue(const V& domainVector, const V* domainDirection, 
		     V* gradVector, M* hessianMatrix, V* hessianEffect) const;
  
  //! Logarithm of the value of the function.
  double lnValue(const V& domainVector, const V* domainDirection, 
		 V* gradVector, M* hessianMatrix, V* hessianEffect) const;

  // NOTE: likely don't need for MCMC:
  //! Computes the logarithm of the normalization factor.
  double computeLogOfNormalizationFactor(unsigned int numSamples,
					 bool m_logOfNormalizationFactor) const;

private:
  using QUESO::BaseScalarFunction<V,M>::m_env;
  using QUESO::BaseScalarFunction<V,M>::m_prefix;
  // using QUESO::BaseScalarFunction<V,M>::m_domainSet;
  // using QUESO::BaseJointPdf<V,M>::m_normalizationStyle;
  // using QUESO::BaseJointPdf<V,M>::m_logOfNormalizationFactor;
  NonDQUESOBayesCalibration* NonDQUESOInstance;
};

// Constructor -------------------------------------
template<class V,class M>
QuesoJointPdf<V,M>::QuesoJointPdf(const char* prefix,
				  const QUESO::VectorSet<V,M>& domainSet,
				  NonDQUESOBayesCalibration*   nond_queso_ptr)
  : QUESO::BaseJointPdf<V,M>(((std::string)(prefix)+"generic").c_str(),
			     domainSet), NonDQUESOInstance(nond_queso_ptr)
{
  if ((m_env.subDisplayFile()) && (m_env.displayVerbosity() >= 54)) {
    *m_env.subDisplayFile() << "Entering QuesoJointPdf<V,M>::constructor()"
                            << ": prefix = " << m_prefix
                            << std::endl;
  }

  if ((m_env.subDisplayFile()) && (m_env.displayVerbosity() >= 54)) {
    *m_env.subDisplayFile() << "Leaving QuesoJointPdf<V,M>::constructor()"
                            << ": prefix = " << m_prefix
                            << std::endl;
  }
}

// Destructor --------------------------------------
template<class V,class M>
QuesoJointPdf<V,M>::~QuesoJointPdf()
{ }

template<class V,class M>
double QuesoJointPdf<V,M>::
actualValue(const V& domainVector, const V* domainDirection,
	    V* gradVector, M* hessianMatrix, V* hessianEffect) const
{ return NonDQUESOInstance->prior_density(domainVector); }
  
template<class V,class M>
double QuesoJointPdf<V,M>::
lnValue(const V& domainVector, const V* domainDirection, 
	V* gradVector, M* hessianMatrix, V* hessianEffect) const
{ return std::log(NonDQUESOInstance->prior_density(domainVector)); }

template<class V,class M>
double QuesoJointPdf<V,M>::
computeLogOfNormalizationFactor(unsigned int numSamples, 
				bool m_logOfNormalizationFactor) const
{ }

/// Dakota specialization of QUESO vector-valued random variable
template <class V, class M>
class QuesoVectorRV: public QUESO::BaseVectorRV<V,M>
{

public:
  //! Default constructor
  /*! Constructs a generic queso vector RV, given a prefix and the
      image set of the vector RV.*/
  QuesoVectorRV(const char*                  prefix,
		const QUESO::VectorSet<V,M>& imageSet,
		NonDQUESOBayesCalibration*   nond_queso_ptr);
  //! Virtual destructor
  virtual ~QuesoVectorRV();
 
  //! TODO: Prints the vector RV (required pure virtual).
  void print(std::ostream& os) const;

private:
  using QUESO::BaseVectorRV<V,M>::m_env;
  using QUESO::BaseVectorRV<V,M>::m_prefix;
  using QUESO::BaseVectorRV<V,M>::m_imageSet;
  using QUESO::BaseVectorRV<V,M>::m_pdf;
  // using QUESO::BaseVectorRV<V,M>::m_realizer;  // only needed to make draws
  // using QUESO::BaseVectorRV<V,M>::m_subCdf;
  // using QUESO::BaseVectorRV<V,M>::m_unifiedCdf;
  // using QUESO::BaseVectorRV<V,M>::m_mdf;
};


// Default constructor-------------------------------
template<class V, class M>
QuesoVectorRV<V,M>::QuesoVectorRV(const char* prefix,
				  const QUESO::VectorSet<V,M>& imageSet,
				  NonDQUESOBayesCalibration*   nond_queso_ptr)
  : QUESO::BaseVectorRV<V,M>(((std::string)(prefix)+"generic").c_str(),imageSet)
{
  if ((m_env.subDisplayFile()) && (m_env.displayVerbosity() >= 54)) {
    *m_env.subDisplayFile() << "Entering QuesoVectorRV<V,M>::constructor()"
                            << ": prefix = " << m_prefix
                            << std::endl;
  }

  m_pdf = new QuesoJointPdf<V,M>(m_prefix.c_str(),
				 m_imageSet, nond_queso_ptr);
  // m_realizer   = NULL; // FIX ME: complete code
  // m_subCdf     = NULL; // FIX ME: complete code
  // m_unifiedCdf = NULL; // FIX ME: complete code
  // m_mdf        = NULL; // FIX ME: complete code

  if ((m_env.subDisplayFile()) && (m_env.displayVerbosity() >= 54)) {
    *m_env.subDisplayFile() << "Leaving QuesoVectorRV<V,M>::constructor()"
                            << ": prefix = " << m_prefix
                            << std::endl;
  }
}

// Destructor ---------------------------------------
template<class V, class M>
QuesoVectorRV<V,M>::~QuesoVectorRV()
{
  // delete m_mdf;
  // delete m_unifiedCdf;
  // delete m_subCdf;
  // delete m_realizer;
  delete m_pdf;
}

// I/O methods --------------------------------------
template <class V, class M>
void
QuesoVectorRV<V,M>::print(std::ostream& os) const
{
  os << "QuesoVectorRV<V,M>::print() says, 'Please implement me.'" << std::endl;
  return;
}


// initialization of statics
NonDQUESOBayesCalibration* NonDQUESOBayesCalibration::NonDQUESOInstance(NULL);

/** This constructor is called for a standard letter-envelope iterator 
    instantiation.  In this case, set_db_list_nodes has been called and 
    probDescDB can be queried for settings from the method specification. */
NonDQUESOBayesCalibration::
NonDQUESOBayesCalibration(ProblemDescDB& problem_db, Model& model):
  NonDBayesCalibration(problem_db, model),
  propCovarType(probDescDB.get_string("method.nond.proposal_cov_type")),
  propCovarData(probDescDB.get_rv("method.nond.proposal_covariance_data")),
  propCovarFilename(probDescDB.get_string("method.nond.proposal_cov_filename")),
  mcmcType(probDescDB.get_string("method.nond.mcmc_type")),
  // these two deprecated:
  likelihoodScale(probDescDB.get_real("method.likelihood_scale")),
  calibrateSigmaFlag(probDescDB.get_bool("method.nond.calibrate_sigma"))
{
  ////////////////////////////////////////////////////////
  // Step 1 of 5: Instantiate the QUESO environment 
  ////////////////////////////////////////////////////////
  init_queso_environment();
 
  // Read in all of the experimental data:  any x configuration 
  // variables, y observations, and y_std if available 
  bool calc_sigma_from_data = true; // calculate sigma if not provided
  expData.load_data("QUESO Bayes Calibration", calc_sigma_from_data);
  
  if (calibrateSigmaFlag) {
    Cerr << "\nError: calibration of sigma temporarily unsupported."<<std::endl;
    abort_handler(-1);
  }

  // for now, assume that if you are reading in any experimental 
  // standard deviations, you do NOT want to calibrate sigma terms
  // BMA TODO: this is wrong logic: need to check for != "none"
  // Also, need to sync up with data available in the covariance class.
  if (!varianceTypesRead.empty() && !calibrateSigmaFlag)
    calibrateSigmaFlag = false;
  // For now, set calcSigmaFlag to true: this should be read from input
  //calibrateSigmaFlag = true;
}


NonDQUESOBayesCalibration::~NonDQUESOBayesCalibration()
{ }


/** Perform the uncertainty quantification */
void NonDQUESOBayesCalibration::quantify_uncertainty()
{
  // instantiate QUESO objects and execute
  NonDQUESOInstance = this;

  // construct emulatorModel and initialize tranformations, as needed
  initialize_model();

  ////////////////////////////////////////////////////////
  // Step 2 of 5: Instantiate the parameter domain
  ////////////////////////////////////////////////////////
  init_parameter_domain();
  // initialize or update the proposal covariance; default init must
  // be done after parameter domain is initialized
  // TODO: In general if user gives proposal covariance; must be
  // transformed to scaled space in same way as variables
  if (!propCovarType.empty()) // either filename OR data values will be defined
    user_proposal_covariance(propCovarType, propCovarData, propCovarFilename);
  else if (!emulatorType)
    default_proposal_covariance();

  // init likelihoodFunctionObj, prior/posterior random vectors, inverse problem
  init_queso_solver();

  switch (adaptPosteriorRefine) {
  case false:
    run_chain_with_restarting();
    break;
  case true:
    if (!emulatorType) { // current spec prevents this
      Cerr << "Error: adaptive posterior refinement requires emulator model."
	   << std::endl;
      abort_handler(-1);
    }
    compactMode = true; // update_model() uses all{Samples,Responses}
    Real adapt_metric = DBL_MAX; unsigned short int num_iter = 0;
    while (adapt_metric > convergenceTol && num_iter < maxIterations) {

      // place update block here so that chain is always run for initial or
      // updated emulator; placing block at loop end could result in emulator
      // convergence w/o final chain.
      if (num_iter) {
	// update the emulator surrogate data with new truth evals and
	// reconstruct surrogate (e.g., via PCE sparse recovery)
	update_model();
	// assess posterior convergence via convergence of the emulator coeffs
	adapt_metric = assess_emulator_convergence();
      }

      // execute MCMC chain, optionally in batches
      run_chain_with_restarting();
      ++num_iter;

      // assess convergence of the posterior via sample-based K-L divergence:
      //adapt_metric = assess_posterior_convergence();
    }
    break;
  }
}


void NonDQUESOBayesCalibration::run_chain_with_restarting()
{
  if (outputLevel >= NORMAL_OUTPUT) {
    if (proposalUpdates > 1)
      Cout << "Running chain in batches of " << numSamples << " with "
	   << proposalUpdates << " restarts." << std::endl;
    else
      Cout << "Running chain with " << numSamples << " samples." << std::endl;
  }

  //Real restart_metric = DBL_MAX;
  size_t prop_update_cntr = 0; unsigned short batch_size = 5;
  //copy_data(emulatorModel.continuous_variables(), prevCenter);
  update_chain_size(numSamples);

  // update proposal covariance and recenter after short chain: a
  // workaround for inability to update proposal covariance on the fly
  while (//restart_metric > convergenceTol &&
	 prop_update_cntr < proposalUpdates) {

    // define proposal covariance from misfit Hessian
    if (emulatorType) // TO DO: support GN Hessian for truth model w/ adjoints?
      precondition_proposal();
    ++prop_update_cntr;

    run_queso_solver(); // solve inverse problem with MCMC 

    if (adaptPosteriorRefine) // extract best MCMC samples from current batch
      filter_chain(prop_update_cntr, batch_size);

    // account for redundancy between final and initial
    if (prop_update_cntr == 1)
      update_chain_size(numSamples+1);

    // This approach is too greedy and can get stuck (i.e., no point in new
    // chain has smaller mismatch than current optimal value):
    //restart_metric = update_center(allSamples[last_index]);
    //
    // Rather, use final point in acceptance chain as if we were periodically
    // refreshing the proposal covariance within a single integrated chain.
    if (prop_update_cntr < proposalUpdates || adaptPosteriorRefine)
      update_center();

    if (outputLevel >= NORMAL_OUTPUT)
      Cout << std::endl;
  }
}


void NonDQUESOBayesCalibration::init_queso_environment()
{
  // NOTE:  for now we are assuming that DAKOTA will be run with 
  // mpiexec to call MPI_Init.  Eventually we need to generalize this 
  // and send QUESO the proper MPI subenvironments.

  // TODO: see if this can be a local, or if the env retains a pointer
  envOptionsValues.reset(new QUESO::EnvOptionsValues());
  envOptionsValues->m_subDisplayFileName = "outputData/display";
  envOptionsValues->m_subDisplayAllowedSet.insert(0);
  envOptionsValues->m_subDisplayAllowedSet.insert(1);
  envOptionsValues->m_displayVerbosity = 2;
  envOptionsValues->m_seed = (randomSeed) ? randomSeed : 1 + (int)clock(); 
 
  if (mcmcType == "multilevel")
    quesoEnv.reset(new QUESO::FullEnvironment(MPI_COMM_SELF,"ml.inp","",NULL));
  else // dram, dr, am, or mh
    quesoEnv.reset(new QUESO::FullEnvironment(MPI_COMM_SELF,"","",
					      envOptionsValues.get()));
}


void NonDQUESOBayesCalibration::init_queso_solver()
{
  ////////////////////////////////////////////////////////
  // Step 3 of 5: Instantiate the likelihood function object
  ////////////////////////////////////////////////////////
  // routine computes [ln(function)]
  likelihoodFunctionObj.reset(new
    QUESO::GenericScalarFunction<QUESO::GslVector,QUESO::GslMatrix>("like_",
    *paramDomain, &dakotaLikelihoodRoutine, (void *) NULL, true));

  ////////////////////////////////////////////////////////
  // Step 4 of 5: Instantiate the inverse problem
  ////////////////////////////////////////////////////////
  // initial approach was restricted to uniform priors
  //priorRv.reset(new QUESO::UniformVectorRV<QUESO::GslVector,QUESO::GslMatrix> 
  //		  ("prior_", *paramDomain));
  // new approach supports arbitrary priors:
  priorRv.reset(new QuesoVectorRV<QUESO::GslVector,QUESO::GslMatrix> 
   		("prior_", *paramDomain, NonDQUESOInstance));

  postRv.reset(new QUESO::GenericVectorRV<QUESO::GslVector,QUESO::GslMatrix>
	       ("post_", *paramSpace));
  // Q: are Prior/posterior RVs copied by StatInvProblem, such that these
  //    instances can be local and allowed to go out of scope?

  // define calIpOptionsValues
  set_ip_options();
  // set options specific to MH algorithm
  set_mh_options();
  // Inverse problem: instantiate it (posterior rv is instantiated internally)
  inverseProb.reset(new
    QUESO::StatisticalInverseProblem<QUESO::GslVector,QUESO::GslMatrix>("",
    calIpOptionsValues.get(), *priorRv, *likelihoodFunctionObj, *postRv));
}


void NonDQUESOBayesCalibration::precondition_proposal()
{
  short asrv;
  switch (emulatorType) {
  case PCE_EMULATOR: case KRIGING_EMULATOR: asrv = 7; break;
  case  SC_EMULATOR: case      GP_EMULATOR: asrv = 3; break; // for now
  }

  // emulatorModel's continuous variables updated in update_center()
  ActiveSet set = emulatorModel.current_response().active_set(); // copy
  set.request_values(asrv);
  emulatorModel.compute_response(set);
  // Note: verbose output *should* echo vars and approx response
  if (outputLevel > NORMAL_OUTPUT)
    Cout << "Parameters for emulator response:\n"
	 << emulatorModel.current_variables()
	 << "\nActive response data:\n" << emulatorModel.current_response()
	 << std::endl;

  // compute Hessian of log-likelihood misfit r^T r (where is Gamma inverse?)
  RealSymMatrix log_like_hess;
  const Response& emulator_resp = emulatorModel.current_response();
  RealMatrix prop_covar;
  if (asrv & 4) { // try to use full misfit Hessian; fall back if indefinite
    build_hessian_of_sum_square_residuals_from_response(emulator_resp,
							log_like_hess);
    bool ev_truncation =
      get_positive_definite_covariance_from_hessian(log_like_hess, prop_covar);
    if (ev_truncation) { // fallback to Gauss-Newton
      build_hessian_of_sum_square_residuals_from_function_gradients(
        emulator_resp.function_gradients(), log_like_hess);
      get_positive_definite_covariance_from_hessian(log_like_hess, prop_covar);
    }
  }
  else { // use Gauss-Newton approximate Hessian
    build_hessian_of_sum_square_residuals_from_function_gradients(
      emulator_resp.function_gradients(), log_like_hess);
    get_positive_definite_covariance_from_hessian(log_like_hess, prop_covar);
  }

  if (outputLevel >= NORMAL_OUTPUT) {
    Cout << "Hessian of misfit (negative log-likelihood):\n";
    write_data(Cout, log_like_hess, true, true, true);
    //Cout << "2x2 determinant = " << log_like_hess(0,0)*log_like_hess(1,1) -
    //  log_like_hess(0,1)*log_like_hess(1,0) << '\n';

    Cout << "Positive definite covariance from inverse of misfit Hessian:\n";
    write_data(Cout, prop_covar, true, true, true);
    //Cout << "2x2 determinant = " << prop_covar(0,0)*prop_covar(1,1) -
    //  prop_covar(0,1)*prop_covar(1,0) << '\n';
  }

  // pack GSL proposalCovMatrix
  int i, j, nv = log_like_hess.numRows();
  if (!proposalCovMatrix) {
    proposalCovMatrix.reset(new QUESO::GslMatrix(paramSpace->zeroVector()));
    if (paramSpace->dimGlobal() != nv) {
      Cerr << "Error: Queso vector space is not consistent with proposal "
	   << "covariance dimension." << std::endl;
      abort_handler(-1);
    }
  }
  for (i=0; i<nv; ++i )
    for (j=0; j<nv; ++j )
      (*proposalCovMatrix)(i,j) = prop_covar(i,j);
}


void NonDQUESOBayesCalibration::run_queso_solver()
{
  Cout << "Running Bayesian Calibration with QUESO using " << mcmcType
       << " with " << calIpMhOptionsValues->m_rawChainSize << " MCMC samples."
       << std::endl;
  //if (outputLevel > NORMAL_OUTPUT)
  //  Cout << "\n  Calibrate Sigma Flag " << calibrateSigmaFlag << std::endl;

  ////////////////////////////////////////////////////////
  // Step 5 of 5: Solve the inverse problem
  ////////////////////////////////////////////////////////
  if (mcmcType == "multilevel")
    inverseProb->solveWithBayesMLSampling();
  else
    inverseProb->solveWithBayesMetropolisHastings(calIpMhOptionsValues.get(),
						  *paramInitials, 
						  proposalCovMatrix.get());

  Cout << "QUESO MCMC chain completed.  QUESO results are in the outputData "
       << "directory:\n  display_sub0.txt contains MCMC diagnostics.\n"
       << "  Matlab files contain the chain values.\n"; //  The files to " 
  //   << "load in Matlab are\nfile_cal_ip_raw.m (the actual chain) " 
  //   << "or file_cal_ip_filt.m (the filtered chain,\nwhich contains " 
  //   << "every 20th step in the chain.\n"
  //   << "NOTE:  the chain values in these Matlab files are currently " 
  //   << "in scaled space. \n  You will have to transform them back to "
  //   << "original space by:\n"
  //   << "lower_bounds + chain_values * (upper_bounds - lower_bounds)\n"
  //   << "The rejection rate is in the tgaCalOutput file.\n"
  //   << "We hope to improve the postprocessing of the chains by the " 
  //   << "next Dakota release.\n";
}


void NonDQUESOBayesCalibration::
filter_chain(size_t update_cntr, unsigned short batch_size)
{
  // filter chain -or- extract full chain and sort on likelihood values
  if (outputLevel >= NORMAL_OUTPUT)
    Cout << "Filtering chain: extracting best " << batch_size
	 << " from MCMC chain " << update_cntr << " containing "
	 << inverseProb->chain().subSequenceSize() << " samples.\n";

  std::/*multi*/map<Real, size_t> local_best;
  chain_to_local(batch_size, local_best);
  if (proposalUpdates > 1) {
    local_to_aggregated(batch_size, local_best);
    if (update_cntr == proposalUpdates)
      aggregated_to_all();
  }
  else
    local_to_all(local_best);
}


void NonDQUESOBayesCalibration::
chain_to_local(unsigned short batch_size, std::map<Real, size_t>& local_best)
{
  // TODO: Need to transform chain back to unscaled space for reporting
  // to user; possibly also in auxilliary data files for user consumption.

  // to get the full acceptance chain, m_filteredChainGenerate is set to
  // false in set_invpb_mh_options()

  // Note: the QUESO VectorSequence class has a number of helpful filtering
  // and statistics functions.

  const QUESO::BaseVectorSequence<QUESO::GslVector,QUESO::GslMatrix>& mcmc_chain
    = inverseProb->chain();
  const QUESO::ScalarSequence<double>& loglike_vals
    = inverseProb->logLikelihoodValues();
  unsigned int num_mcmc = mcmc_chain.subSequenceSize();
  if (num_mcmc != loglike_vals.subSequenceSize()) {
    Cerr << "Error (NonDQUESO): final mcmc chain has length " << num_mcmc 
	 << "\n                 but likelihood set has length"
	 << loglike_vals.subSequenceSize() << std::endl;
    abort_handler(-1);
  }

  // TO DO: want to keep different samples with same likelihood, but not 
  // replicate samples with same likelihood (from rejection); for now, use
  // a std::map since the latter is unlikely.
  QUESO::GslVector mcmc_sample(paramSpace->zeroVector());
  for (size_t chain_pos = 0; chain_pos < num_mcmc; ++chain_pos) {
    // extract GSL sample vector from QUESO vector sequence:
    mcmc_chain.getPositionValues(chain_pos, mcmc_sample);
    // evaluate log of posterior from log likelihood and log prior:
    Real log_prior     = std::log(prior_density(mcmc_sample)),
         log_posterior = loglike_vals[chain_pos] + log_prior;
    //std::log(emulatorModel.continuous_probability_density(mcmc_sample));
    if (outputLevel > NORMAL_OUTPUT)
      Cout << "MCMC sample: " << mcmc_sample << " log prior = " << log_prior
	   << " log posterior = " << log_posterior << std::endl;
    // sort ascending by log posterior (highest prob are last) and retain
    // batch_size samples
    local_best.insert(std::pair<Real, size_t>(log_posterior, chain_pos));
    if (local_best.size() > batch_size)
      local_best.erase(local_best.begin()); // pop front (lowest prob)
  }
  if (outputLevel > NORMAL_OUTPUT)
    Cout << "local_best map:\n" << local_best << std::endl;
}


void NonDQUESOBayesCalibration::
local_to_aggregated(unsigned short batch_size,
		    const std::map<Real, size_t>& local_best)
{
  // Merge local std::map<Real, size_t> into aggregate/class-scope
  // std::map<Real, QUESO::GslVector> 
  const QUESO::BaseVectorSequence<QUESO::GslVector,QUESO::GslMatrix>&
    mcmc_chain = inverseProb->chain();
  std::/*multi*/map<Real, size_t>::const_iterator cit;
  QUESO::GslVector mcmc_sample(paramSpace->zeroVector());
  for (cit=local_best.begin(); cit!=local_best.end(); ++cit) {
    mcmc_chain.getPositionValues(cit->second, mcmc_sample);
    bestSamples.insert(
      std::pair<Real, QUESO::GslVector>(cit->first, mcmc_sample));
    // continuously maintain length to reduce sorting cost
    if (bestSamples.size() > batch_size)
      bestSamples.erase(bestSamples.begin()); // pop front
  }
  if (outputLevel > NORMAL_OUTPUT)
    Cout << "bestSamples map:\n" << bestSamples << std::endl;
}


void NonDQUESOBayesCalibration::aggregated_to_all()
{
  // copy bestSamples into allSamples
  size_t num_best = bestSamples.size();
  if (allSamples.numCols() != num_best)
    allSamples.shapeUninitialized(numContinuousVars, num_best);
  std::/*multi*/map<Real, QUESO::GslVector>::iterator it; size_t i;
  if (outputLevel >= NORMAL_OUTPUT) Cout << "Chain filtering results:\n";
  for (it=bestSamples.begin(), i=0; it!=bestSamples.end(); ++it, ++i) {
    copy_gsl(it->second, allSamples, i);
    if (outputLevel >= NORMAL_OUTPUT) {
      Cout << "Best point " << i+1 << ": Log posterior = " << it->first
	   << " Sample:";
      write_col_vector_trans(Cout, (int)i, false, false, true, allSamples);
    }
  }
  bestSamples.clear();
}


void NonDQUESOBayesCalibration::
local_to_all(const std::map<Real, size_t>& local_best)
{
  // copy local_best into allSamples
  size_t num_best = local_best.size();
  if (allSamples.numCols() != num_best)
    allSamples.shapeUninitialized(numContinuousVars, num_best);
  const QUESO::BaseVectorSequence<QUESO::GslVector,QUESO::GslMatrix>&
    mcmc_chain = inverseProb->chain();
  QUESO::GslVector mcmc_sample(paramSpace->zeroVector());
  std::/*multi*/map<Real, size_t>::const_iterator cit; size_t i;
  if (outputLevel >= NORMAL_OUTPUT) Cout << "Chain filtering results:\n";
  for (cit=local_best.begin(), i=0; cit!=local_best.end(); ++cit, ++i) {
    mcmc_chain.getPositionValues(cit->second, mcmc_sample);
    copy_gsl(mcmc_sample, allSamples, i);
    if (outputLevel >= NORMAL_OUTPUT) {
      Cout << "Best point " << i+1 << ": Log posterior = " << cit->first
	   << " Sample:";
      write_col_vector_trans(Cout, (int)i, false, false, true, allSamples);
    }
  }
}


void NonDQUESOBayesCalibration::update_center()
{
  const QUESO::BaseVectorSequence<QUESO::GslVector,QUESO::GslMatrix>& 
    mcmc_chain = inverseProb->chain();
  unsigned int num_mcmc = mcmc_chain.subSequenceSize();

  // extract GSL sample vector from QUESO vector sequence:
  size_t last_index = num_mcmc - 1;
  mcmc_chain.getPositionValues(last_index, *paramInitials);
  if (outputLevel > NORMAL_OUTPUT)
    Cout << "New center:\n" << *paramInitials << "Log likelihood = "
	 << inverseProb->logLikelihoodValues()[last_index] << std::endl;

  // update emulatorModel vars with end of acceptance chain for eval of
  // misfit Hessian in precondition_proposal().  Note: the most recent
  // emulatorModel evaluation could correspond to a rejected point.
  RealVector c_vars;
  copy_gsl(*paramInitials, c_vars);
  emulatorModel.continuous_variables(c_vars);
}


/*
Real NonDQUESOBayesCalibration::update_center(const RealVector& new_center)
{
  // update emulatorModel vars for eval of misfit Hessian
  emulatorModel.continuous_variables(new_center);

  // update QUESO initial vars for starting point of chain
  for (int i=0; i<numContinuousVars; i++)
    (*paramInitials)[i] = new_center[i];

  // evaluate and return L2 norm of change in chain starting point
  RealVector delta_center = new_center;
  delta_center -= prevCenter;
  prevCenter = new_center;
  return delta_center.normFrobenius();
}
*/


void NonDQUESOBayesCalibration::update_model()
{
  // perform truth evals (in parallel) for selected points
  if (outputLevel >= NORMAL_OUTPUT)
    Cout << "Updating emulator: evaluating " << allSamples.numCols()
	 << " best points." << std::endl;
  evaluate_parameter_sets(iteratedModel, true, false); // log allResponses

  // update emulatorModel with new data from iteratedModel
  if (outputLevel >= NORMAL_OUTPUT)
    Cout << "Updating emulator: appending " << allResponses.size()
	 << " new data sets." << std::endl;
  emulatorModel.append_approximation(allSamples, allResponses, true); // rebuild
}


Real NonDQUESOBayesCalibration::assess_emulator_convergence()
{
  // coeff reference point not yet available; force another iteration rather
  // than use norm of current coeffs (stopping on small norm is not meaningful)
  if (prevCoeffs.empty()) {
    switch (emulatorType) {
    case PCE_EMULATOR:
      prevCoeffs = emulatorModel.approximation_coefficients(true);  break;
    case SC_EMULATOR:
      prevCoeffs = emulatorModel.approximation_coefficients(false); break;
    case GP_EMULATOR: case KRIGING_EMULATOR:
      Cerr << "Warning: convergence norm not yet defined for GP emulators in "
	   << "NonDQUESOBayesCalibration::assess_emulator_convergence()."
	   << std::endl;
      break;
    }
    return DBL_MAX;
  }

  Real l2_norm_delta_coeffs = 0., delta_coeff_ij;
  switch (emulatorType) {
  case PCE_EMULATOR: {
    const RealVectorArray& coeffs
      = emulatorModel.approximation_coefficients(true); // normalized
    size_t i, j, num_qoi = coeffs.size(), num_coeffs_i;

    // This approach requires an unchanged multiIndex, which is acceptable 
    // for regression PCE using a fixed (candidate) expansion definition.
    // Sparsity is not a concern as returned coeffs are inflated to be
    // dense with respect to SharedOrthogPolyApproxData::multiIndex.
    for (i=0; i<num_qoi; ++i) {
      const RealVector&      coeffs_i =     coeffs[i];
      const RealVector& prev_coeffs_i = prevCoeffs[i];
      num_coeffs_i = coeffs_i.length();
      for (j=0; j<num_coeffs_i; ++j) {
	delta_coeff_ij = coeffs_i[j] - prev_coeffs_i[j];
	l2_norm_delta_coeffs += delta_coeff_ij * delta_coeff_ij;
      }
    }

    prevCoeffs = coeffs;
    break;
  }
  case SC_EMULATOR: {
    // Interpolation could use a similar concept with the expansion coeffs,
    // although adaptation would imply differences in the grid.
    const RealVectorArray& coeffs
      = emulatorModel.approximation_coefficients(false);

    Cerr << "Warning: convergence norm not yet defined for SC emulator in "
	 << "NonDQUESOBayesCalibration::assess_emulator_convergence()."
	 << std::endl;
    //abort_handler(-1);
    return DBL_MAX;
    break;
  }
  case GP_EMULATOR: case KRIGING_EMULATOR:
    // Consider use of correlation lengths.
    // TO DO: define SurfpackApproximation::approximation_coefficients()...
    Cerr << "Warning: convergence norm not yet defined for GP emulators in "
	 << "NonDQUESOBayesCalibration::assess_emulator_convergence()."
	 << std::endl;
    //abort_handler(-1);
    return DBL_MAX;
    break;
  }

  if (outputLevel >= NORMAL_OUTPUT) {
    Real norm = std::sqrt(l2_norm_delta_coeffs);
    Cout << "Assessing emulator convergence: l2 norm = " << norm << std::endl;
    return norm;
  }
  else
    return std::sqrt(l2_norm_delta_coeffs);
}


void NonDQUESOBayesCalibration::init_parameter_domain()
{
  int total_num_params = (calibrateSigmaFlag) ?
    numContinuousVars + numFunctions : numContinuousVars; 
  
  paramSpace.reset(new QUESO::VectorSpace<QUESO::GslVector,QUESO::GslMatrix>
		   (*quesoEnv, "param_", total_num_params, NULL));

  QUESO::GslVector paramMins(paramSpace->zeroVector()),
                   paramMaxs(paramSpace->zeroVector());
  for (size_t i=0; i<numContinuousVars; i++) {
    RealRealPair bnds = emulatorModel.continuous_distribution_bounds(i);
    paramMins[i] = bnds.first; paramMaxs[i] = bnds.second;
  }
  // if calibrateSigmaFlag, the parameter domain is expanded by sigma terms 
  if (calibrateSigmaFlag) {
    RealVector covariance_diagonal;
    // Todo: pass in corrrect experiment number as second argument
    expData.get_main_diagonal( covariance_diagonal, 0 );
    for (int j=0; j<numFunctions; j++){
      // TODO: Need to be able to get sigma from new covariance class.
      // Also need to sync up this with the logic in the ctor.  Also need
      // a default if there's no experimental data (may not be sensible)
      //paramMins[numContinuousVars+j] = 1.;
      //paramMaxs[numContinuousVars+j] = 1.;
      Real std_j = std::sqrt( covariance_diagonal[j] );
      paramMins[numContinuousVars+j] = .01 * std_j;
      paramMaxs[numContinuousVars+j] = 2.  * std_j;
    }
  }
  paramDomain.reset(new QUESO::BoxSubset<QUESO::GslVector,QUESO::GslMatrix>
		    ("param_", *paramSpace, paramMins, paramMaxs));

  paramInitials.reset(new QUESO::GslVector(paramSpace->zeroVector()));
  const RealVector& init_point = emulatorModel.continuous_variables();
  for (size_t i=0; i<numContinuousVars; i++)
    (*paramInitials)[i] = init_point[i];
  if (calibrateSigmaFlag)
    for (size_t i=numContinuousVars; i<total_num_params; i++)
      (*paramInitials)[i] = (paramMaxs[i] + paramMins[i]) / 2.;

  if (outputLevel > NORMAL_OUTPUT)
    Cout << "Initial Parameter values sent to QUESO (may be in scaled)\n"
	 << *paramInitials << "\nParameter bounds sent to QUESO (may be scaled)"
	 << ":\nparamMins " << paramMins << "\nparamMaxs " << paramMaxs << '\n';
}


/** Must be called after paramMins/paramMaxs set above */
void NonDQUESOBayesCalibration::default_proposal_covariance()
{
  int total_num_params = paramSpace->dimGlobal();
  QUESO::GslVector covDiag(paramSpace->zeroVector());

  // default to covariance of independent uniforms (diagonal)
  const QUESO::GslVector& param_min = paramDomain->minValues();
  const QUESO::GslVector& param_max = paramDomain->maxValues();
  for (int i=0; i<total_num_params; i++)
    covDiag[i] = (param_max[i]-param_min[i])*(param_max[i]-param_min[i])/12.0;
  
  if (outputLevel > NORMAL_OUTPUT)
    Cout << "Diagonal elements of the proposal covariance " 
	 << "sent to QUESO (may be in scaled space) \n" << covDiag << '\n';

  proposalCovMatrix.reset(new QUESO::GslMatrix(covDiag));
  
  if (outputLevel > NORMAL_OUTPUT) {
    Cout << "QUESO ProposalCovMatrix " << '\n'; 
    for (size_t i=0; i<total_num_params; i++) {
      for (size_t j=0; j<total_num_params; j++) 
	Cout <<  (*proposalCovMatrix)(i,j) << "  "; 
      Cout << '\n'; 
    }
  }
}


void NonDQUESOBayesCalibration::
user_proposal_covariance(const String& cov_type, 
			 const RealVector& cov_data, 
			 const String& cov_filename)
{
  // TODO: Will need to scale user covariance if this method is
  // applying its own scaling.

  // this function will convert user-specified cov_type = "diagonal" |
  // "matrix" data from either cov_data or cov_filename and populate a
  // full QUESO::GslMatrix* in proposalCovMatrix with the covariance

  bool use_file = !cov_filename.empty();

  // Sanity check
  if( ("diagonal" != cov_type) &&
      ("matrix"   != cov_type) )
    throw std::runtime_error("User-specified covariance must have type of either \"diagonal\" of \"matrix\".  You have \""+cov_type+"\".");

  // Sanity check
  if( cov_data.length() && use_file )
    throw std::runtime_error("You cannot provide both covariance values and a covariance data filename.");

  // Size our Queso covariance matrix
  proposalCovMatrix.reset(new QUESO::GslMatrix(paramSpace->zeroVector()));

  // Sanity check
  int total_num_params = paramSpace->dimGlobal();
  if( (proposalCovMatrix->numRowsLocal()  != total_num_params) || 
      (proposalCovMatrix->numRowsGlobal() != total_num_params) || 
      (proposalCovMatrix->numCols()       != total_num_params)   )
    throw std::runtime_error("Queso vector space is not consistent with parameter dimension.");
        
  // Read in a general way and then check that the data is consistent
  RealVectorArray values_from_file;
  if( use_file )
  {
    std::ifstream s;
    TabularIO::open_file(s, cov_filename, "read_queso_covariance_data");
    bool row_major = false;
    read_unsized_data(s, values_from_file, row_major);
  }

  if( "diagonal" == cov_type )
  {
    if( use_file ) {
      // Sanity checks
      if( values_from_file.size() != 1 ) 
        throw std::runtime_error("\"diagonal\" Queso covariance file data should have 1 column and "
                                 +convert_to_string(total_num_params)+" rows.");
      if( values_from_file[0].length() != total_num_params )
        throw std::runtime_error("\"diagonal\" Queso covariance file data should have "
                                 +convert_to_string(total_num_params)+" rows.  Found "
                                 +convert_to_string(values_from_file[0].length())+" rows.");
      for( int i=0; i<total_num_params; ++i )
        (*proposalCovMatrix)(i,i) = values_from_file[0](i);
    }
    else {
      // Sanity check
      if( total_num_params != cov_data.length() )
        throw std::runtime_error("Expected num covariance values is "+convert_to_string(total_num_params)
                                 +" but incoming vector provides "+convert_to_string(cov_data.length())+".");
      for( int i=0; i<total_num_params; ++i )
        (*proposalCovMatrix)(i,i) = cov_data(i);
    }
  }
  else // "matrix" == cov_type
  {
    if( use_file ) {
      // Sanity checks
      if( values_from_file.size() != total_num_params ) 
        throw std::runtime_error("\"matrix\" Queso covariance file data should have "
                                 +convert_to_string(total_num_params)+" columns.  Found "
                                 +convert_to_string(values_from_file.size())+" columns.");
      if( values_from_file[0].length() != total_num_params )
        throw std::runtime_error("\"matrix\" Queso covariance file data should have "
                                 +convert_to_string(total_num_params)+" rows.  Found "
                                 +convert_to_string(values_from_file[0].length())+" rows.");
      for( int i=0; i<total_num_params; ++i )
        for( int j=0; j<total_num_params; ++j )
          (*proposalCovMatrix)(i,j) = values_from_file[i](j);
    }
    else {
      // Sanity check
      if( total_num_params*total_num_params != cov_data.length() )
        throw std::runtime_error("Expected num covariance values is "+convert_to_string(total_num_params*total_num_params)
            +" but incoming vector provides "+convert_to_string(cov_data.length())+".");
      int count = 0;
      for( int i=0; i<total_num_params; ++i )
        for( int j=0; j<total_num_params; ++j )
          (*proposalCovMatrix)(i,j) = cov_data[count++];
    }
  }

  // validate that provided data is a valid covariance - test symmetry
  //    Note: I had hoped to have this check occur in the call to chol() below, 
  //    but chol doesn't seem to mind that matrices are not symmetric... RWH
  //proposalCovMatrix->print(std::cout);
  //std::cout << std::endl;
  QUESO::GslMatrix test_mat = proposalCovMatrix->transpose();
  test_mat -= *proposalCovMatrix;
  if( test_mat.normMax() > 1.e-14 )
    throw std::runtime_error("Queso covariance matrix is not symmetric.");

  // validate that provided data is a valid covariance matrix - test PD part of SPD
  test_mat = *proposalCovMatrix;
  int ierr = test_mat.chol();
  if( ierr == QUESO::UQ_MATRIX_IS_NOT_POS_DEFINITE_RC)
    throw std::runtime_error("Queso covariance data is not SPD.");

  //proposalCovMatrix->print(std::cout);
  //std::cout << std::endl;
  //cov_data.print(std::cout);
}


/// set inverse problem options common to all solvers
void NonDQUESOBayesCalibration::set_ip_options() 
{
  calIpOptionsValues.reset(new QUESO::SipOptionsValues());
  //definitely want to retain computeSolution
  calIpOptionsValues->m_computeSolution    = true;
  calIpOptionsValues->m_dataOutputFileName = "outputData/invpb_output";
  calIpOptionsValues->m_dataOutputAllowedSet.insert(0);
  calIpOptionsValues->m_dataOutputAllowedSet.insert(1);
}


void NonDQUESOBayesCalibration::set_mh_options() 
{
  calIpMhOptionsValues.reset(new QUESO::MhOptionsValues());

  calIpMhOptionsValues->m_dataOutputFileName = "outputData/mh_output";
  calIpMhOptionsValues->m_dataOutputAllowedSet.insert(0);
  calIpMhOptionsValues->m_dataOutputAllowedSet.insert(1);

  calIpMhOptionsValues->m_rawChainDataInputFileName   = ".";
  calIpMhOptionsValues->m_rawChainSize = (numSamples) ? numSamples : 48576;
  //calIpMhOptionsValues->m_rawChainGenerateExtra     = false;
  //calIpMhOptionsValues->m_rawChainDisplayPeriod     = 20000;
  //calIpMhOptionsValues->m_rawChainMeasureRunTimes   = true;
  calIpMhOptionsValues->m_rawChainDataOutputFileName  = "outputData/raw_chain";
  calIpMhOptionsValues->m_rawChainDataOutputAllowedSet.insert(0);
  calIpMhOptionsValues->m_rawChainDataOutputAllowedSet.insert(1);
  // NO LONGER SUPPORTED.  calIpMhOptionsValues->m_rawChainComputeStats = true;

  //calIpMhOptionsValues->m_displayCandidates         = false;
  calIpMhOptionsValues->m_putOutOfBoundsInChain       = false;
  //calIpMhOptionsValues->m_tkUseLocalHessian         = false;
  //calIpMhOptionsValues->m_tkUseNewtonComponent      = true;

  // delayed rejection option:
  calIpMhOptionsValues->m_drMaxNumExtraStages
    = (mcmcType == "delayed_rejection" || mcmcType == "dram") ? 1 : 0;
  calIpMhOptionsValues->m_drScalesForExtraStages.resize(1);
  calIpMhOptionsValues->m_drScalesForExtraStages[0] = 5;
  //calIpMhOptionsValues->m_drScalesForExtraStages[1] = 10;
  //calIpMhOptionsValues->m_drScalesForExtraStages[2] = 20;

  // adaptive metropolis option:
  calIpMhOptionsValues->m_amInitialNonAdaptInterval
    = (mcmcType == "adaptive_metropolis" || mcmcType == "dram") ? 1 : 0;
  calIpMhOptionsValues->m_amAdaptInterval           = 100;
  calIpMhOptionsValues->m_amEta                     = 2.88;
  calIpMhOptionsValues->m_amEpsilon                 = 1.e-8;

  // chain management options:
  if (adaptPosteriorRefine || proposalUpdates > 1)
    // In this case, we process the full chain for maximum posterior values
    calIpMhOptionsValues->m_filteredChainGenerate         = false;
  else {
    // In this case, we filter the chain for final output
    calIpMhOptionsValues->m_filteredChainGenerate         = true;
    calIpMhOptionsValues->m_filteredChainDiscardedPortion = 0.;
    calIpMhOptionsValues->m_filteredChainLag              = 20;
    calIpMhOptionsValues->
      m_filteredChainDataOutputFileName = "outputData/filtered_chain";
    calIpMhOptionsValues->m_filteredChainDataOutputAllowedSet.insert(0);
    calIpMhOptionsValues->m_filteredChainDataOutputAllowedSet.insert(1);
    //calIpMhOptionsValues->m_filteredChainComputeStats   = true;
  }

  // logit transform addresses high rejection rates in corners of bounded
  // domains.  It is hardwired on at this time, although potentially redundant
  // in some cases (e.g., WIENER u-space type).
  calIpMhOptionsValues->m_doLogitTransform = false;
}


void NonDQUESOBayesCalibration::update_chain_size(unsigned int size)
{ if (size) calIpMhOptionsValues->m_rawChainSize = size; }


//void NonDQUESOBayesCalibration::print_results(std::ostream& s)
//{
//  NonDBayesCalibration::print_results(s);
//
//  additional QUESO output
//}


double NonDQUESOBayesCalibration::dakotaLikelihoodRoutine(
  const QUESO::GslVector& paramValues, const QUESO::GslVector* paramDirection,
  const void*         functionDataPtr, QUESO::GslVector*       gradVector,
  QUESO::GslMatrix*     hessianMatrix, QUESO::GslVector*       hessianEffect)
{
  double result = 0.;
  size_t i, j;
  int num_exp = NonDQUESOInstance->numExperiments,
      num_fn  = NonDQUESOInstance->numFunctions,
      num_cv  = NonDQUESOInstance->numContinuousVars; 

  // The GP/KRIGING/NO EMULATOR may use an unstandardized space (original)
  // and the PCE or SC cases always use standardized space.
  //
  // We had discussed having QUESO search in the original space:  this may 
  // difficult for high dimensional spaces depending on the scaling, 
  // because QUESO calculates the volume of the hypercube in which it is 
  // searching and will stop if it is too small (e.g. if one input is 
  // of small magnitude, searching in the original space will not be viable).

  //RealVector x; NonDQUESOInstance->copy_gsl(paramValues, x);
  //NonDQUESOInstance->emulatorModel.continuous_variables(x);
  for (i=0; i<num_cv; ++i)
    NonDQUESOInstance->emulatorModel.continuous_variable(paramValues[i], i);

  NonDQUESOInstance->emulatorModel.compute_response();

  const Response& resp = NonDQUESOInstance->emulatorModel.current_response();
  const RealVector& fn_vals = resp.function_values();
  if (NonDQUESOInstance->outputLevel >= DEBUG_OUTPUT)
    Cout << "input is " << paramValues << "\noutput is " << fn_vals << '\n';

  // TODO: Update treatment of standard deviations as inference
  // vs. fixed parameters; also advanced use cases of calibrated
  // scalar sigma against user-provided covariance structure.

  // Calculate the likelihood depending on what information is
  // available for the standard deviations NOTE: If the calibration of
  // the sigma terms is included, we assume ONE sigma term per
  // function is calibrated.  Otherwise, we assume that yStdData has
  // already had the correct values placed depending if there is zero,
  // one, num_fn, or a full num_exp*num_fn matrix of standard deviations.
  // Thus, we just have to iterate over this to calculate the likelihood.
  if (NonDQUESOInstance->calibrateSigmaFlag) {
    for (i=0; i<num_exp; i++) {
      const RealVector& exp_data = NonDQUESOInstance->expData.all_data(i);
      for (j=0; j<num_fn; j++)
        result += pow((fn_vals[j]-exp_data[j])/paramValues[num_cv+j],2.);
    }
  }
  else {
    RealVector residuals;
    for (i=0; i<num_exp; i++) {
      NonDQUESOInstance->expData.form_residuals(resp, i, residuals);
      result += NonDQUESOInstance->expData.apply_covariance(residuals, i);
    }
  }

  result /= -2. * NonDQUESOInstance->likelihoodScale;
  if (NonDQUESOInstance->outputLevel >= DEBUG_OUTPUT)
    Cout << "Log likelihood is " << result << " Likelihood is " << exp(result)
	 << '\n';
  
  // TODO: open file once and append here, or rely on QUESO to output
  if (NonDQUESOInstance->outputLevel > NORMAL_OUTPUT) {
    std::ofstream QuesoOutput;
    QuesoOutput.open("QuesoOutput.txt", std::ios::out | std::ios::app);
    for (i=0; i<num_cv; i++) QuesoOutput << paramValues[i] << ' ' ;
    for (j=0; j<num_fn; j++) QuesoOutput << fn_vals[j] << ' ' ;
    QuesoOutput << result << '\n';
    QuesoOutput.close();
  }
  return result;
}


void NonDQUESOBayesCalibration::
copy_gsl(const QUESO::GslVector& qv, RealVector& rv)
{
  size_t i, size_qv = qv.sizeLocal();
  if (size_qv != rv.length())
    rv.sizeUninitialized(size_qv);
  for (i=0; i<size_qv; ++i)
    rv[i] = qv[i];
}


void NonDQUESOBayesCalibration::
copy_gsl(const QUESO::GslVector& qv, RealMatrix& rm, int col)
{
  size_t i, size_qv = qv.sizeLocal();
  if (col < 0 || col >= rm.numCols() || size_qv != rm.numRows()) {
    Cerr << "Error: inconsistent matrix access in copy_gsl()." << std::endl;
    abort_handler(-1);
  }
  Real* rm_c = rm[col];
  for (i=0; i<size_qv; ++i)
    rm_c[i] = qv[i];
}


Real NonDQUESOBayesCalibration::prior_density(const QUESO::GslVector& qv)
{
  // TO DO: consider QUESO-based approach for this using priorRv.pdf(),
  // which may in turn call back to our GenericVectorRV prior plug-in

  // Mirrors Model::continuous_probability_density() for efficiency:
  // > avoid incurring overhead of GslVector -> RealVector copy
  // > avoid repeated dist_index lookups when looping over single var version

  // error trap on correlated random variables
  const Pecos::AleatoryDistParams& a_dist
    = emulatorModel.aleatory_distribution_parameters();
  if (!a_dist.uncertain_correlations().empty()) {
    Cerr << "Error: prior_density() uses a product of marginal densities\n"
	 << "       and can only be used for independent random variables."
	 << std::endl;
    abort_handler(-1);
  }

  UShortMultiArrayConstView cv_types
    = emulatorModel.continuous_variable_types();
  Real pdf = 1.; size_t i, dist_index = 0, num_cv = cv_types.size();
  for (i=0; i<num_cv; ++i) {
    // design/epistemic/state return 1
    pdf *= emulatorModel.continuous_probability_density(qv[i], cv_types[i],
							dist_index);
    // shortcut: increment distribution index if same type, else reset
    if (i+1 < num_cv)
      dist_index = (cv_types[i] == cv_types[i+1]) ? dist_index + 1 : 0;
  }
  return pdf;
}

} // namespace Dakota
