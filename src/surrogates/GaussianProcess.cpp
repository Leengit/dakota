/*  _______________________________________________________________________

    DAKOTA: Design Analysis Kit for Optimization and Terascale Applications
    Copyright 2014 Sandia Corporation.
    This software is distributed under the GNU Lesser General Public License.
    For more information, see the README file in the top Dakota directory.
    _______________________________________________________________________ */

#include "GaussianProcess.hpp"

#include "Teuchos_ParameterList.hpp"
#include "Teuchos_XMLParameterListHelpers.hpp"
#include "Teuchos_oblackholestream.hpp"

#include "GP_Objective.hpp"
#include "ROL_Algorithm.hpp"
#include "ROL_Bounds.hpp"

namespace dakota {

using util::NormalizationScaler;
using util::NoScaler;
using util::StandardizationScaler;

namespace surrogates {

GaussianProcess::GaussianProcess(){}
GaussianProcess::~GaussianProcess(){}

void GaussianProcess::set_theta(const std::vector<double> theta_new) {
  for (int i = 0; i < thetaValues.size(); i++)
    thetaValues(i) = theta_new[i];
}

void GaussianProcess::generate_initial_guesses(MatrixXd &initial_guesses, int num_restarts,
                                               const VectorXd &sigma_bounds,
                                               const MatrixXd &length_scale_bounds) {
  initial_guesses = MatrixXd::Random(num_restarts,numVariables+1);
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
}


void GaussianProcess::compute_prediction_matrix(const MatrixXd &pred_pts, MatrixXd &pred_mat) {
  const int numPredictionPts = pred_pts.rows();
  pred_mat.resize(numPredictionPts,numSamples);
  MatrixXd scaled_samples = dataScaler->getScaledFeatures();
  for (int i = 0; i < numPredictionPts; i++) {
    for (int j = 0; j < numSamples; j++) {
      pred_mat(i,j) = sq_exp_cov_pred(pred_pts.row(i),scaled_samples.row(j));
    }
  }
}

double GaussianProcess::sq_exp_cov(const int i, const int j) {
  double inside = 0.0;
  for (int k = 0; k < numVariables; k++)
    inside += componentwiseDistances[k](i,j)*std::exp(-2.0*thetaValues(k+1));

  return std::exp(2.0*thetaValues(0))*exp(-0.5*inside);
}

double GaussianProcess::sq_exp_cov_pred(const VectorXd &x, const VectorXd &y) {
  VectorXd diff = x - y;
  double inside = 0.0;
  for (int k = 0; k < numVariables; k++)
    inside += std::pow(diff(k),2.0)*std::exp(-2.0*thetaValues(k+1));

  return std::exp(2.0*thetaValues(0))*std::exp(-0.5*inside);
}
                

void GaussianProcess::compute_Gram(bool compute_derivs){
  for (int i = 0; i < numSamples; i++) {
    for (int j = i; j < numSamples; j++) {
      GramMatrix(i,j) = sq_exp_cov(i,j);
      if (i != j)
        GramMatrix(j,i) = GramMatrix(i,j);
    }
  }

  if (compute_derivs) {
    GramMatrixDerivs[0] = 2.0*GramMatrix;
    for (int k = 1 ; k < numVariables + 1; k++) {
      GramMatrixDerivs[k] = GramMatrix.cwiseProduct(componentwiseDistances[k-1])
                            *std::exp(-2.0*thetaValues(k));
    }
  }

  /* add in the nugget */
  for (int i = 0; i < numSamples; i++)
    GramMatrix(i,i) += nuggetValue;
}

void GaussianProcess::negative_marginal_log_likelihood(double &obj_value, VectorXd &obj_gradient) {
  double logSum;
  VectorXd z;
  MatrixXd Q, eye_matrix;
  eye_matrix = MatrixXd::Identity(numSamples,numSamples);

  compute_Gram(true);
  CholFact.compute(GramMatrix);
  VectorXd D(CholFact.vectorD());

  z = CholFact.solve(targetValues);
  logSum = 0.5*log(D.array()).matrix().sum();
  Q = -0.5*(z*z.transpose() - CholFact.solve(eye_matrix));

  obj_value = logSum + 0.5*targetValues.col(0).dot(z) + 
              static_cast<double>(numSamples)/2.0*log(2.0*PI);

  MatrixXd product(numSamples,numSamples);
  for (int k = 0; k < numVariables+1; k++)
    obj_gradient(k) = (GramMatrixDerivs[k].cwiseProduct(Q)).sum();
}


void GaussianProcess::compute_Gram_pred(const MatrixXd &samples, MatrixXd &Gram_pred) {
  const int numPredictionPts = samples.rows();
  Gram_pred.resize(numPredictionPts,numPredictionPts);
  for (int i = 0; i < numPredictionPts; i++) {
    for (int j = i; j < numPredictionPts; j++) {
      Gram_pred(i,j) = sq_exp_cov_pred(samples.row(i),samples.row(j));
      if (i != j)
        Gram_pred(j,i) = Gram_pred(i,j);
    }
  }

}

GaussianProcess::GaussianProcess(const MatrixXd &samples, 
                                 const MatrixXd &response, 
                                 const VectorXd &sigma_bounds,
                                 const MatrixXd &length_scale_bounds,
                                 const std::string scaler_type,
                                 const int num_restarts,
                                 const double nugget_val,
                                 const int seed) {

  numSamples = samples.rows();
  numVariables = samples.cols();
  targetValues = response;
  
  /* Scale the data */
  if (scaler_type == "mean normalization")
    dataScaler = std::make_shared<NormalizationScaler>(samples,true);
  else if (scaler_type == "min-max normalization")
    dataScaler = std::make_shared<NormalizationScaler>(samples,false);
  if (scaler_type == "standardization")
    dataScaler = std::make_shared<StandardizationScaler>(samples);
  else if (scaler_type == "none" )
    dataScaler = std::make_shared<NoScaler>(samples);
  else
    throw(std::string("Invalid scaler type"));

  MatrixXd scaled_samples = dataScaler->getScaledFeatures();

  /* size of thetaValues for squared exponential kernel and one QoI */
  thetaValues.resize(numVariables+1);
  bestThetaValues.resize(numVariables+1);
  /* set the size of the GramMatrix and its derivatives */
  GramMatrix.resize(numSamples, numSamples);
  GramMatrixDerivs.resize(numVariables+1);
  for (int k = 0; k < numVariables+1; k++) {
    GramMatrixDerivs[k].resize(numSamples, numSamples);
  }

  /* set the nugget */
  nuggetValue = nugget_val;

  /* compute Euclidean distances squared */
  componentwiseDistances.resize(numVariables);
  for (int k = 0; k < numVariables; k++) {
    componentwiseDistances[k].resize(numSamples,numSamples);
    for (int i = 0; i < numSamples; i++) {
      for (int j = i; j < numSamples; j++) {
        componentwiseDistances[k](i,j) = std::pow(scaled_samples(i,k) - scaled_samples(j,k), 2.0);
        if (i != j)
          componentwiseDistances[k](j,i) = componentwiseDistances[k](i,j);
      }
    }
  }


  /* set up the initial guesses */
  srand(seed);
  MatrixXd initial_guesses;
  generate_initial_guesses(initial_guesses, num_restarts,
                           sigma_bounds, length_scale_bounds);

  ROL::Ptr<std::ostream> outStream;
  Teuchos::oblackholestream bhs;
  outStream = ROL::makePtrFromRef(bhs);
  //outStream = ROL::makePtrFromRef(std::cout);
  std::string paramfile = "rol_params.xml";
  auto rol_params = Teuchos::rcp(new Teuchos::ParameterList);
  Teuchos::updateParametersFromXmlFile(paramfile, rol_params.ptr());
  auto gp_objective = std::make_shared<GP_Objective>(this);
  int dim = numVariables + 1;
  ROL::Algorithm<double> algo("Line Search",*rol_params);

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

  ROL::Ptr<ROL::Vector<double> > lop = ROL::makePtr<ROL::StdVector<double> >(lo_ptr);
  ROL::Ptr<ROL::Vector<double> > hip = ROL::makePtr<ROL::StdVector<double> >(hi_ptr);
  bound = ROL::makePtr<ROL::Bounds<double> >(lop,hip);

  std::vector<std::string> output;

  objectiveFunctionHistory.resize(num_restarts);
  bestObjFunValue = 1.0e300;

  double final_obj_value;
  VectorXd final_obj_gradient(dim);

  for (int i = 0; i < num_restarts; i++) {
    for (int j = 0; j < dim; ++j) {
      (*x_ptr)[j] = initial_guesses(i,j);
    }
    output = algo.run(x, *gp_objective, *bound, true, *outStream);
    for (int j = 0; j < thetaValues.size(); ++j) {
      thetaValues(j) = (*x_ptr)[j];
    }
    /* get the final objective function value */
    negative_marginal_log_likelihood(final_obj_value, final_obj_gradient);
    if (final_obj_value < bestObjFunValue) {
      bestObjFunValue = final_obj_value;
      bestThetaValues = thetaValues;
    }
    objectiveFunctionHistory(i) = final_obj_value;
    algo.reset();
  }

  thetaValues = bestThetaValues;

  /*
  std::cout << "\n";
  std::cout << objectiveFunctionHistory << std::endl;
  std::cout << "optimal theta values in log-space:" << std::endl;
  std::cout << bestThetaValues << std::endl;
  std::cout << "best objective function value is " <<  bestObjFunValue << std::endl;
  std::cout << "best objective function gradient norm is " <<  final_obj_gradient.norm() << std::endl;
  */

}

void GaussianProcess::value(const MatrixXd &samples, MatrixXd &approx_values) {
  const int numPredictionPts = samples.rows();
  if (numPredictionPts != approx_values.rows()) {
    throw(std::runtime_error("Gaussian Process value inputs are not consistent."
          " Number of samples and approximation sizes do not match"));
  }

  /* scale the samples (prediction points) */
  MatrixXd scaled_pred_pts = dataScaler->scaleSamples(samples);

  /* compute the Gram matrix and its Cholesky factorization */
  compute_Gram(false);
  CholFact.compute(GramMatrix);

  MatrixXd pred_mat;
  compute_prediction_matrix(scaled_pred_pts,pred_mat);

  MatrixXd chol_solve_target, chol_solve_pred_mat;
  chol_solve_target = CholFact.solve(targetValues);
  chol_solve_pred_mat = CholFact.solve(pred_mat.transpose());

  /* value */
  approx_values = pred_mat*chol_solve_target;

  /* compute the covariance matrix and standard deviation */
  MatrixXd Gram_pred;
  compute_Gram_pred(scaled_pred_pts,Gram_pred);
  posteriorCov = Gram_pred - pred_mat*chol_solve_pred_mat;

  posteriorStdDev.resize(numPredictionPts);
  for (int i = 0; i < numPredictionPts; i++) {
    posteriorStdDev(i) = sqrt(posteriorCov(i,i));
  }

}

}  // namespace surrogates
}  // namespace dakota

