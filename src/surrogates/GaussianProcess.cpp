/*  _______________________________________________________________________

    DAKOTA: Design Analysis Kit for Optimization and Terascale Applications
    Copyright 2014-2020 National Technology & Engineering Solutions of Sandia, LLC (NTESS).
    This software is distributed under the GNU Lesser General Public License.
    For more information, see the README file in the top Dakota directory.
    _______________________________________________________________________ */

#include "GaussianProcess.hpp"
#include "GP_Objective.hpp"

#include "ROL_Algorithm.hpp"
#include "ROL_Bounds.hpp"
#include "ROL_LineSearchStep.hpp"

#include "Teuchos_oblackholestream.hpp"
#include "Teuchos_XMLParameterListHelpers.hpp"

namespace dakota {
namespace surrogates {

GaussianProcess::GaussianProcess(){
  default_options();
}

GaussianProcess::GaussianProcess(const ParameterList &param_list) {
  default_options();
  configOptions = param_list;
}

// BMA NOTE: ParameterList::get() can throw, so direct delegation
// probably not good; might want to give a helpful message
GaussianProcess::GaussianProcess(const MatrixXd &samples,
				 const MatrixXd &response,
				 const ParameterList& param_list) {
  default_options();
  configOptions = param_list;
  build(samples, response);
}

GaussianProcess::~GaussianProcess(){}

void GaussianProcess::build(const MatrixXd &samples, const MatrixXd &response)
{
  configOptions.validateParametersAndSetDefaults(defaultConfigOptions);
  std::cout << "\nBuilding GaussianProcess with configuration options\n"
	  << configOptions << "\n";

  numQOI = response.cols();
  numSamples = samples.rows();
  numVariables = samples.cols();
  targetValues = response;
  eyeMatrix = MatrixXd::Identity(numSamples,numSamples);

  /* Optimization-related data*/
  VectorXd sigma_bounds = configOptions.get<VectorXd>("sigma bounds");
  MatrixXd length_scale_bounds = configOptions.get<MatrixXd>("length-scale bounds");

  /* Optional data*/
  estimateTrend = configOptions.sublist("Trend").get<bool>("estimate trend");
  estimateNugget = configOptions.sublist("Nugget").get<bool>("estimate nugget");
  MatrixXd beta_bounds;
  VectorXd nugget_bounds;

  const int num_restarts = configOptions.get<int>("num restarts");
  
  /* Scale the data and compute build squared distances */
  dataScaler = util::scaler_factory(
    util::DataScaler::scaler_type(configOptions.get<std::string>("scaler name")),
    samples);
  const MatrixXd& scaled_samples = dataScaler->get_scaled_features();
  compute_build_dists();

  if (estimateTrend) {
    polyRegression = std::make_shared<PolynomialRegression>
                     (scaled_samples, targetValues,
                      configOptions.sublist("Trend").sublist("Options"));
    numPolyTerms = polyRegression->get_num_terms();
    polyRegression->compute_basis_matrix(scaled_samples, basisMatrix);
    beta_bounds = MatrixXd::Ones(numPolyTerms,2);
    beta_bounds.col(0) *= -betaBound;
    beta_bounds.col(1) *= betaBound;
  }

  /* size of thetaValues for squared exponential kernel and one QoI */
  thetaValues.resize(numVariables+1);
  bestThetaValues.resize(numVariables+1);
  betaValues.resize(numPolyTerms);
  bestBetaValues.resize(numPolyTerms);
  /* set the size of the GramMatrix and its derivatives */
  GramMatrix.resize(numSamples, numSamples);
  GramMatrixDerivs.resize(numVariables+1);
  for (int k = 0; k < numVariables+1; k++) {
    GramMatrixDerivs[k].resize(numSamples, numSamples);
  }

  fixedNuggetValue = configOptions.sublist("Nugget").get<double>("fixed nugget");
  /* DTS: if the nugget is being estimated, should the fixed value be set to zero? */
  if (estimateNugget) {
    nugget_bounds = configOptions.sublist("Nugget").get<VectorXd>("nugget bounds");
    numNuggetTerms = 1;
  }

  /* set up the initial guesses */
  srand(configOptions.get<int>("gp seed"));
  MatrixXd initial_guesses;
  generate_initial_guesses(sigma_bounds, length_scale_bounds,
                           nugget_bounds, num_restarts,
                           initial_guesses);

  ROL::Ptr<std::ostream> outStream;
  Teuchos::oblackholestream bhs;
  outStream = ROL::makePtrFromRef(bhs);

  /* Uncomment if you'd like to print ROL's output to screen.
   * Useful for debugging */
  //outStream = ROL::makePtrFromRef(std::cout);

  /* No more reading in rol_params from an xml file
   * Set defaults in here instead */
  /*
  std::string paramfile = "rol_params.xml";
  auto rol_params = Teuchos::rcp(new ParameterList);
  Teuchos::updateParametersFromXmlFile(paramfile, rol_params.ptr());
  */

  auto gp_mle_rol_params = Teuchos::rcp(new ParameterList("GP_MLE_Optimization"));
  setup_default_optimization_params(gp_mle_rol_params);

  auto gp_objective = std::make_shared<GP_Objective>(*this);
  int dim = numVariables + 1 + numPolyTerms + numNuggetTerms;

  // Define algorithm
  ROL::Ptr<ROL::Step<double>> step = 
    ROL::makePtr<ROL::LineSearchStep<double>>(*gp_mle_rol_params);
  ROL::Ptr<ROL::StatusTest<double>>
    status = ROL::makePtr<ROL::StatusTest<double>>(*gp_mle_rol_params);
  ROL::Algorithm<double> algo(step, status, false);

  /* set up parameter vectors and bounds */
  ROL::Ptr<std::vector<double> > x_ptr = ROL::makePtr<std::vector<double>>(dim, 0.0);
  ROL::StdVector<double> x(x_ptr);
  ROL::Ptr<ROL::Bounds<double> > bound;
  ROL::Ptr<std::vector<double> > lo_ptr = ROL::makePtr<std::vector<double> >(dim,0.0);
  ROL::Ptr<std::vector<double> > hi_ptr = ROL::makePtr<std::vector<double> >(dim,0.0);
  /* sigma bounds */
  (*lo_ptr)[0] = log(sigma_bounds(0));
  (*hi_ptr)[0] = log(sigma_bounds(1));
  /* length scale bounds */
  for (int i = 0; i < numVariables; i++) {
    (*lo_ptr)[i+1] = log(length_scale_bounds(i,0));
    (*hi_ptr)[i+1] = log(length_scale_bounds(i,1));
  }
  if (estimateTrend) {
    for (int i = 0; i < numPolyTerms; i++) {
      (*lo_ptr)[numVariables+1+i] = beta_bounds(i,0);
      (*hi_ptr)[numVariables+1+i] = beta_bounds(i,1);
    }
  }
  if (estimateNugget) {
    (*lo_ptr)[dim-1] = log(nugget_bounds(0));
    (*hi_ptr)[dim-1] = log(nugget_bounds(1));
  }

  ROL::Ptr<ROL::Vector<double> > lop = ROL::makePtr<ROL::StdVector<double> >(lo_ptr);
  ROL::Ptr<ROL::Vector<double> > hip = ROL::makePtr<ROL::StdVector<double> >(hi_ptr);
  bound = ROL::makePtr<ROL::Bounds<double> >(lop,hip);

  std::vector<std::string> output;

  objectiveFunctionHistory.resize(num_restarts);

  double final_obj_value;
  VectorXd final_obj_gradient(dim);

  for (int i = 0; i < num_restarts; i++) {
    for (int j = 0; j < dim; ++j) {
      (*x_ptr)[j] = initial_guesses(i,j);
    }
    output = algo.run(x, *gp_objective, *bound, true, *outStream);
    for (int j = 0; j < thetaValues.size(); ++j) {
      (thetaValues)(j) = (*x_ptr)[j];
    }
    if (estimateTrend) {
      for (int j = 0; j < numPolyTerms; ++j) {
        betaValues(j) = (*x_ptr)[numVariables+1+j];
      }
    }
    if (estimateNugget) {
      estimatedNuggetValue = (*x_ptr)[numVariables+1+numPolyTerms];
    }
    /* get the final objective function value */
    negative_marginal_log_likelihood(final_obj_value, final_obj_gradient);
    if (final_obj_value < bestObjFunValue) {
      bestObjFunValue = final_obj_value;
      bestThetaValues = thetaValues;
      if (estimateTrend)
        bestBetaValues = betaValues;
      if (estimateNugget)
        bestEstimatedNuggetValue = estimatedNuggetValue;
    }
    objectiveFunctionHistory(i) = final_obj_value;
    algo.reset();
  }

  thetaValues = bestThetaValues;
  if (estimateTrend) {
    betaValues = bestBetaValues;
    /* set the betas in the polynomialRegression class */
    polyRegression->set_polynomial_coeffs(bestBetaValues);
  }
  if (estimateNugget)
    estimatedNuggetValue = bestEstimatedNuggetValue;

  /* Useful info for debugging */
  /*
  std::cout << "\n";
  std::cout << objectiveFunctionHistory << "\n";
  std::cout << "optimal theta values in log-space:" << "\n";
  std::cout << bestThetaValues << "\n";
  std::cout << "best objective function value is " <<  bestObjFunValue << "\n";
  std::cout << "best objective function gradient norm is " <<  final_obj_gradient.norm() << "\n";
  */
}

void GaussianProcess::value(const MatrixXd &samples, MatrixXd &approx_values) {
  if (samples.cols() != numVariables) {
    throw(std::runtime_error("Gaussian Process value inputs are not consistent."
          " Dimension of the feature space for the evaluation points and Gaussian Process do not match"));
  }

  const int numPredictionPts = samples.rows();
  approx_values.resize(numPredictionPts,numVariables);

  /* scale the samples (prediction points) */
  MatrixXd scaled_pred_pts;
  dataScaler->scale_samples(samples, scaled_pred_pts);
  compute_pred_dists(scaled_pred_pts);

  /* compute the Gram matrix and its Cholesky factorization */
  compute_gram(cwiseDists2, true, false, GramMatrix);
  CholFact.compute(GramMatrix);

  MatrixXd resid, pred_basis_matrix, mixed_pred_gram;
  compute_gram(cwiseMixedDists2, false, false, mixed_pred_gram);

  if (estimateTrend) {
    resid = targetValues - basisMatrix*betaValues;
  }
  else
    resid = targetValues;


  MatrixXd chol_solve_resid, chol_solve_pred_mat;
  chol_solve_resid = CholFact.solve(resid);
  chol_solve_pred_mat = CholFact.solve(mixed_pred_gram.transpose());

  /* value */
  approx_values = mixed_pred_gram*chol_solve_resid;

  /* compute the covariance matrix and standard deviation */
  MatrixXd pred_gram;
  compute_gram(cwisePredDists2, true, false, pred_gram);
  posteriorCov = pred_gram - mixed_pred_gram*chol_solve_pred_mat;

  if (estimateTrend) {
    polyRegression->compute_basis_matrix(scaled_pred_pts, pred_basis_matrix);
    MatrixXd z = CholFact.solve(basisMatrix);
    MatrixXd R_mat = pred_basis_matrix - mixed_pred_gram*(z);
    MatrixXd h_mat = basisMatrix.transpose()*z;
    approx_values += pred_basis_matrix*betaValues;
    posteriorCov += R_mat*(h_mat.ldlt().solve(R_mat.transpose()));
  }

  posteriorStdDev = posteriorCov.diagonal().array().sqrt();
}

void GaussianProcess::gradient(const MatrixXd &samples, MatrixXd &gradient,
                               const int qoi) {
  silence_unused_args(qoi);

  // Surrogate models don't yet support multiple responses
  assert(qoi == 0);

  if (samples.cols() != numVariables) {
    throw(std::runtime_error("Gaussian Process gradient inputs are not consistent."
          " Dimension of the feature space for the evaluation points and Gaussian Process do not match"));
  }

  const int numPredictionPts = samples.rows();

  /* resize the gradient */
  gradient.resize(numPredictionPts, numVariables);

  /* scale the samples (prediction points) */
  MatrixXd scaled_pred_pts;
  dataScaler->scale_samples(samples, scaled_pred_pts);
  compute_pred_dists(scaled_pred_pts);

  /* compute the Gram matrix and its Cholesky factorization */
  compute_gram(cwiseDists2, true, false, GramMatrix);
  CholFact.compute(GramMatrix);

  MatrixXd mixed_pred_gram, chol_solve_resid, first_deriv_pred_gram,
           grad_components, resid;
  compute_gram(cwiseMixedDists2, false, false, mixed_pred_gram);
  resid = targetValues;
  if (estimateTrend)
    resid -= basisMatrix*betaValues;
  chol_solve_resid = CholFact.solve(resid);

  for (int i = 0; i < numVariables; i++) {
    compute_first_deriv_pred_gram(mixed_pred_gram, i, first_deriv_pred_gram);
    grad_components = first_deriv_pred_gram*chol_solve_resid;
    gradient.col(i) = grad_components.col(0);
  }

  /* extra terms for GP with a trend */
  if (estimateTrend) {
    MatrixXd poly_grad_pred_pts;
    polyRegression->gradient(scaled_pred_pts, poly_grad_pred_pts);
    gradient += poly_grad_pred_pts;
  }
}

void GaussianProcess::hessian(const MatrixXd &sample, MatrixXd &hessian,
                              const int qoi) {
  silence_unused_args(qoi);

  // Surrogate models don't yet support multiple responses
  assert(qoi == 0);

  if (sample.rows() != 1) {
    throw(std::runtime_error("Gaussian Process Hessian evaluation is for a single point."
          "The input contains more than one sample."));
  }

  /* resize the Hessian */
  hessian.resize(numVariables, numVariables);

  /* scale the samples (prediction points) */
  MatrixXd scaled_pred_point;
  dataScaler->scale_samples(sample, scaled_pred_point);
  compute_pred_dists(scaled_pred_point);

  /* compute the Gram matrix and its Cholesky factorization */
  compute_gram(cwiseDists2, true, false, GramMatrix);
  CholFact.compute(GramMatrix);

  MatrixXd mixed_pred_gram, chol_solve_resid, second_deriv_pred_gram, resid;
  compute_gram(cwiseMixedDists2, false, false, mixed_pred_gram);
  resid = targetValues;
  if (estimateTrend)
    resid -= basisMatrix*betaValues;
  chol_solve_resid = CholFact.solve(resid);

  /* Hessian */
  for (int i = 0; i < numVariables; i++) {
    for (int j = i; j < numVariables; j++) {
      compute_second_deriv_pred_gram(mixed_pred_gram, i, j, second_deriv_pred_gram);
      hessian(i,j) = (second_deriv_pred_gram*chol_solve_resid)(0,0);
      if (i != j)
        hessian(j,i) = hessian(i,j);
    }
  }

  if (estimateTrend) {
    MatrixXd poly_hessian_pred_pt;
    polyRegression->hessian(scaled_pred_point, poly_hessian_pred_pt);
    hessian += poly_hessian_pred_pt;
  }
}

void GaussianProcess::negative_marginal_log_likelihood(double &obj_value, VectorXd &obj_gradient) {
  double logSum;
  VectorXd z;
  MatrixXd Q, resid;

  compute_gram(cwiseDists2, true, true, GramMatrix);
  CholFact.compute(GramMatrix);
  VectorXd D(CholFact.vectorD());

  if (estimateTrend) {
    resid = targetValues - basisMatrix*betaValues;
    z = CholFact.solve(resid);
    obj_gradient.segment(numVariables+1, numPolyTerms) = 
                         -basisMatrix.transpose()*z;
  }
  else {
    resid = targetValues;
    z = CholFact.solve(resid);
  }

  logSum = 0.5*log(D.array()).matrix().sum();
  Q = -0.5*(z*z.transpose() - CholFact.solve(eyeMatrix));

  obj_value = logSum + 0.5*(resid.transpose()*z)(0,0) + 
              static_cast<double>(numSamples)/2.0*log(2.0*PI);

  for (int k = 0; k < numVariables+1; k++)
    obj_gradient(k) = (GramMatrixDerivs[k].cwiseProduct(Q)).sum();

  if (estimateNugget) {
    obj_gradient(numVariables+1+numPolyTerms) = 2.0*exp(2.0*estimatedNuggetValue)
                                              * Q.trace();
  }
}

const VectorXd& GaussianProcess::get_posterior_std_dev() const { return posteriorStdDev; }

const MatrixXd& GaussianProcess::get_posterior_covariance() const { return posteriorCov; }

int GaussianProcess::get_num_opt_variables() {
  return numVariables + 1 + numPolyTerms + numNuggetTerms;
}

int GaussianProcess::get_num_variables() const { return numVariables; }

void GaussianProcess::set_opt_params(const std::vector<double> &opt_params) {
  for (int i = 0; i < numVariables + 1; i++) 
    thetaValues(i) = opt_params[i];

  if (estimateTrend) {
    for (int i = 0; i < numPolyTerms; i++)
      betaValues(i) = opt_params[numVariables+1+i];
  }

  if (estimateNugget)
    estimatedNuggetValue = opt_params[numVariables+1+numPolyTerms];
}

void GaussianProcess::default_options()
{
  // BMA TODO: with both approaches, how to publish a list of all valid options?

  // bound constraints -- will be converted to log-scale
  // sigma bounds - lower and upper
  VectorXd sigma_bounds(2);
  sigma_bounds(0) = 1.0e-2;
  sigma_bounds(1) = 1.0e2;
  // length scale bounds - num_vars x 2
  MatrixXd length_scale_bounds(1,2);
  length_scale_bounds(0,0) = 1.0e-2;
  length_scale_bounds(0,1) = 1.0e2;

  /* results in a nugget**2 betwen 1.0e-15 and 1.0e-8 */
  VectorXd nugget_bounds(2);
  nugget_bounds(0) = 3.17e-8;
  nugget_bounds(1) = 1.0e-2;

  defaultConfigOptions.set("sigma bounds", sigma_bounds, "sigma [lb, ub]");
  // BMA: Do we want to allow 1 x 2 always as a fallback?
  defaultConfigOptions.set("length-scale bounds", length_scale_bounds, "length scale num_vars x [lb, ub]");
  defaultConfigOptions.set("scaler name", "mean normalization", "scaler for variables");
  defaultConfigOptions.set("num restarts", 5, "local optimizer number of initial iterates");
  defaultConfigOptions.set("gp seed", 129, "random seed for initial iterate generation");
  /* Nugget */
  defaultConfigOptions.sublist("Nugget")
                      .set("fixed nugget", 0.0, "fixed nugget term");
  defaultConfigOptions.sublist("Nugget")
                      .set("estimate nugget", false, "estimate a nugget term");
  defaultConfigOptions.sublist("Nugget")
                      .set("nugget bounds", nugget_bounds, "nugget term [lb, ub]");
  /* Polynomial Trend */
  defaultConfigOptions.sublist("Trend").set("estimate trend", false, "estimate a trend term");
  defaultConfigOptions.sublist("Trend").sublist("Options")
                      .set("max degree", 2, "Maximum polynomial order");
  defaultConfigOptions.sublist("Trend").sublist("Options")
                      .set("p-norm", 1.0, "P-Norm in hyperbolic cross");
  defaultConfigOptions.sublist("Trend").sublist("Options")
                      .set("scaler type", "none", "Type of data scaling");
  defaultConfigOptions.sublist("Trend").sublist("Options")
                      .set("regression solver type", "SVD", "Type of regression solver");
}

void GaussianProcess::compute_build_dists() {

  const MatrixXd& scaled_samples = dataScaler->get_scaled_features();
  cwiseDists2.resize(numVariables);

  for (int k = 0; k < numVariables; k++) {
    cwiseDists2[k].resize(numSamples,numSamples);
    for (int i = 0; i < numSamples; i++) {
      for (int j = i; j < numSamples; j++) {
        cwiseDists2[k](i,j) = pow(scaled_samples(i,k) - scaled_samples(j,k), 2);
        if (i != j)
          cwiseDists2[k](j,i) = cwiseDists2[k](i,j);
      }
    }
  }
}

void GaussianProcess::compute_pred_dists(const MatrixXd &scaled_pred_pts) {

  const int num_pred_pts = scaled_pred_pts.rows();
  cwiseMixedDists.resize(numVariables);
  cwiseMixedDists2.resize(numVariables);
  cwisePredDists2.resize(numVariables);
  const MatrixXd& scaled_samples = dataScaler->get_scaled_features();

  for (int k = 0; k < numVariables; k++) {
    cwiseMixedDists[k].resize(num_pred_pts, numSamples);
    cwisePredDists2[k].resize(num_pred_pts, num_pred_pts);
    for (int i = 0; i < num_pred_pts; i++) {
      for (int j = 0; j < numSamples; j++) {
        cwiseMixedDists[k](i,j) = scaled_pred_pts(i,k) - scaled_samples(j,k);
      }
      for (int j = i; j < num_pred_pts; j++) {
        cwisePredDists2[k](i,j) = pow(scaled_pred_pts(i,k) - scaled_pred_pts(j,k), 2);
        if (i != j)
          cwisePredDists2[k](j,i) = cwisePredDists2[k](i,j);
      }
    }
    cwiseMixedDists2[k] = cwiseMixedDists[k].array().square();
  }
}

void GaussianProcess::compute_gram(const std::vector<MatrixXd> &dists2, bool add_nugget, 
                                   bool compute_derivs, MatrixXd &gram) {
  for (int k = 0; k < numVariables; k++) {
    if (k == 0)
      gram = dists2[k]*exp(-2.0*thetaValues[k+1]);
    else
      gram += dists2[k]*exp(-2.0*thetaValues[k+1]);
  }
  gram = exp(2.0*thetaValues[0])*(-0.5*gram.array()).exp();

  if (compute_derivs) {
    GramMatrixDerivs[0] = 2.0*gram;
    for (int k = 1 ; k < numVariables + 1; k++) {
      GramMatrixDerivs[k] = gram.cwiseProduct(dists2[k-1])
                            *exp(-2.0*(thetaValues)(k));
    }
  }

  if (add_nugget) {
    /* add in the fixed nugget */
    gram.diagonal().array() += fixedNuggetValue;
    /* add in the estimated nugget */
    if (estimateNugget)
      gram.diagonal().array() += exp(2.0*estimatedNuggetValue);
  }
}


void GaussianProcess::compute_first_deriv_pred_gram(const MatrixXd &pred_gram, const int index, MatrixXd &first_deriv_pred_gram) {
  first_deriv_pred_gram = -pred_gram.cwiseProduct(cwiseMixedDists[index])
                        * exp(-2.0*thetaValues[index+1]);
}

void GaussianProcess::compute_second_deriv_pred_gram(const MatrixXd &pred_gram,
                                                     const int index_i, const int index_j,
                                                     MatrixXd &second_deriv_pred_gram) {
  double delta_ij = 1.0;
  if (index_i != index_j)
    delta_ij = 0.0;

  second_deriv_pred_gram = pred_gram.array()*exp(-2.0*thetaValues[index_i+1])
    * ((cwiseMixedDists[index_i].cwiseProduct(cwiseMixedDists[index_j])).array()
    * exp(-2.0*thetaValues[index_j+1]) - delta_ij);

}

void GaussianProcess::generate_initial_guesses(const VectorXd &sigma_bounds,
                                               const MatrixXd &length_scale_bounds,
                                               const VectorXd &nugget_bounds,
                                               const int num_restarts,
                                               MatrixXd &initial_guesses) {

  initial_guesses = MatrixXd::Random(num_restarts,numVariables + 1
                    + numPolyTerms + numNuggetTerms);
  double mean, span;
  for (int j = 0; j < numVariables+1; j++) {
    if (j == 0) {
      span = 0.5*(log(sigma_bounds(1)) - log(sigma_bounds(0)));
      mean = 0.5*(log(sigma_bounds(1)) + log(sigma_bounds(0)));
    }
    else {
      span = 0.5*(log(length_scale_bounds(j-1,1)) - log(length_scale_bounds(j-1,0)));
      mean = 0.5*(log(length_scale_bounds(j-1,1)) + log(length_scale_bounds(j-1,0)));
    }
    for (int i = 0; i < num_restarts; i++) {
      initial_guesses(i,j) = span*initial_guesses(i,j) + mean;
    }
  }
  /* Initial guess for trend terms set to zero */
  if (estimateTrend) {
    int index_offset = numVariables + 1;
    for (int i = 0; i < num_restarts; ++i) {
      for (int j = 0; j < numPolyTerms; j++) {
        initial_guesses(i, index_offset + j) = 0.0;
      }
    }
  }
  if (estimateNugget) {
    int index_offset = numVariables + 1 + numPolyTerms;
    span = 0.5*(log(nugget_bounds(1)) - log(nugget_bounds(0)));
    mean = 0.5*(log(nugget_bounds(1)) + log(nugget_bounds(0)));
    for (int i = 0; i < num_restarts; ++i) {
      initial_guesses(i,index_offset) = span*initial_guesses(i,index_offset)
                                        + mean;
    }
  }
}

void GaussianProcess::setup_default_optimization_params(
     Teuchos::RCP<ParameterList> rol_params) {
  /* Secant */
  rol_params->sublist("General").sublist("Secant").
             set("Type","Limited-Memory BFGS");
  rol_params->sublist("General").sublist("Secant").
             set("Maximum Storage",25);
  /* Step */ 
  rol_params->sublist("General").sublist("Step").
             sublist("Line Search").set("Function Evaluation Limit",20);

  rol_params->sublist("General").sublist("Step").
             sublist("Line Search").set("Sufficient Decrease Tolerance",1.0e-4);

  rol_params->sublist("General").sublist("Step").
             sublist("Line Search").set("Initial Step Size",1.0);

  rol_params->sublist("General").sublist("Step").
             sublist("Line Search").sublist("Descent Method").
             set("Type","Quasi-Newton");

  rol_params->sublist("General").sublist("Step").
             sublist("Line Search").sublist("Descent Method").
             set("Nonlinear CG Type","Hestenes-Stiefel");

  rol_params->sublist("General").sublist("Step").
             sublist("Line Search").sublist("Curvature Condition").
             set("Type","Strong Wolfe Conditions");

  rol_params->sublist("General").sublist("Step").
             sublist("Line Search").sublist("Curvature Condition").
             set("General Parameter",0.9);

  rol_params->sublist("General").sublist("Step").
             sublist("Line Search").sublist("Curvature Condition").
             set("Generalized Wolfe Parameter",0.6);

  rol_params->sublist("General").sublist("Step").
             sublist("Line Search").sublist("Line-Search Method").
             set("Type","Cubic Interpolation");

  rol_params->sublist("General").sublist("Step").
             sublist("Line Search").sublist("Line-Search Method").
             set("Backtracking Rate",0.5);

  rol_params->sublist("General").sublist("Step").
             sublist("Line Search").sublist("Line-Search Method").
             set("Bracketing Tolerance",1.0e-8);

  /* Status Test */
  rol_params->sublist("Status Test").
             set("Gradient Tolerance",1.0e-6);

  rol_params->sublist("Status Test").
             set("Step Tolerance",1.0e-8);

  rol_params->sublist("Status Test").
             set("Iteration Limit",200);
}

}  // namespace surrogates
}  // namespace dakota

