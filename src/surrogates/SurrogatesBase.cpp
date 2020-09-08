/*  _______________________________________________________________________

    DAKOTA: Design Analysis Kit for Optimization and Terascale Applications
    Copyright 2014-2020 National Technology & Engineering Solutions of Sandia, LLC (NTESS).
    This software is distributed under the GNU Lesser General Public License.
    For more information, see the README file in the top Dakota directory.
    _______________________________________________________________________ */

#include "SurrogatesBase.hpp"
#include "util_metrics.hpp"
#include "util_math_tools.hpp"


namespace dakota {
namespace surrogates {

Surrogate::Surrogate() : numQOI(0) {}

Surrogate::Surrogate(const ParameterList &param_list) {
  numQOI = 0;
  silence_unused_args(param_list);
}


Surrogate::Surrogate(const MatrixXd &samples,
		             		 const MatrixXd &response,
				             const ParameterList& param_list) {
  numQOI = 0;
  silence_unused_args(samples, response, param_list);
}

Surrogate::~Surrogate(){}

/* VectorXd is the appropriate return type for single qoi evaluation */
VectorXd Surrogate::value(const MatrixXd &samples, const int qoi) {
  MatrixXd approx_values;
  gradient(samples, approx_values, qoi);
  Eigen::Map<VectorXd> vec_map(approx_values.data(), samples.rows(), 1);
  return vec_map;
}

/* returns num_samples by num_vars for a given qoi */
void Surrogate::gradient(const MatrixXd &samples, MatrixXd &gradient, int qoi) {
  silence_unused_args(samples, gradient, qoi);
  throw(std::runtime_error("Surrogate does not implement gradient(...)"));
}

MatrixXd Surrogate::gradient(const MatrixXd &samples, int qoi) {
  MatrixXd approx_gradient;
  gradient(samples, approx_gradient, qoi);
  return approx_gradient;
}


/* returns num_vars by num_vars for a single sample and a given qoi */
void Surrogate::hessian(const MatrixXd &sample, MatrixXd &hessian, int qoi) {
  silence_unused_args(sample, hessian, qoi);
  throw(std::runtime_error("Surrogate does not implement hessian(...)"));
}

MatrixXd Surrogate::hessian(const MatrixXd &sample, int qoi) {
  MatrixXd approx_hessian;
  hessian(sample, approx_hessian, qoi);
  return approx_hessian;
}

void Surrogate::set_options(const ParameterList &options) {
  configOptions = options;
}

void Surrogate::get_options(ParameterList &options) {
  options = configOptions;
}

/// Evalute metrics at specified points (within surrogates)
VectorXd Surrogate::evaluate_metrics(const StringArray &mnames,
  const MatrixXd &points, const MatrixXd &ref_values) {

  const int num_metrics = mnames.size();
  /* Assuming numQOI = 1 */
  MatrixXd surr_values(ref_values.size(),1);
  VectorXd metrics(num_metrics);

  for (int m = 0; m < num_metrics; m++) {
    this->value(points, surr_values);
    metrics(m) = util::compute_metric(surr_values.col(0),
      ref_values.col(0), mnames[m]);
  }
  return metrics;
}

/// Evalute metrics at specified points (from Dakota)
/* DTS: HAS NOT BEEN TESTED */
RealMatrix Surrogate::evaluate_metrics(const StringArray &mnames,
  const RealMatrix &points, const RealMatrix &ref_values) {

  const int num_metrics = mnames.size();
  /* Assuming numQOI = 1 */
  Eigen::Map<MatrixXd> eigen_points(points.values(), points.numRows(),
				    points.numCols());
  Eigen::Map<MatrixXd> eigen_ref_values(ref_values.values(),
					ref_values.numRows(),
					ref_values.numCols());
  VectorXd eigen_metrics = evaluate_metrics(mnames, eigen_points,
    eigen_ref_values);

  RealMatrix metrics(Teuchos::Copy, eigen_metrics.data(),
    num_metrics, num_metrics, 1);

  return metrics;
}

/// K-folds cross validation (within surrogates)
VectorXd Surrogate::cross_validate(const MatrixXd &samples,
    const MatrixXd &response, const StringArray &mnames,
    const int num_folds, const int seed) {

  const int num_metrics = mnames.size();
  VectorXd cv_results = VectorXd::Zero(num_metrics);
  VectorXd metrics_values(num_metrics);

  const int num_samples = samples.rows();
  const int num_features = samples.cols();
  std::vector<VectorXi> cv_folds;
  VectorXi fold_indices;
  MatrixXd train_samples, train_response;
  MatrixXd val_samples, val_response;

  int num_train_samples, num_val_samples;
  int samples_index, train_index;

  util::create_cv_folds(num_folds, num_samples, cv_folds, seed);

  // clone the surrogate's configuration so CV doesn't invalidate *this
  std::shared_ptr<Surrogate> cv_surrogate = this->clone();

  for (int i = 0; i < num_folds; i++) {
    /* validation samples */
    fold_indices = cv_folds[i];
    num_val_samples = fold_indices.size();
    val_samples.resize(num_val_samples, num_features);
    val_response.resize(num_val_samples, 1);
    for (int j = 0; j < num_val_samples; j++) {
      samples_index = fold_indices(j);
      val_samples.row(j) = samples.row(samples_index);
      val_response(j,0) = response(samples_index ,0);
    }

    /* training samples */
    num_train_samples = num_samples - num_val_samples;
    train_samples.resize(num_train_samples, num_features);
    train_response.resize(num_train_samples, 1);
    train_index = 0;
    for (int k = 0; k < num_folds; k++) {
      if (k != i) {
        fold_indices = cv_folds[k];
        for (int j = 0; j < fold_indices.size(); j++) {
          samples_index = fold_indices(j);
          train_samples.row(train_index) = samples.row(samples_index);
          train_response(train_index, 0) = response(samples_index, 0);
          train_index++;
        }
      }
    }

    cv_surrogate->build(train_samples, train_response);
    metrics_values =
      cv_surrogate->evaluate_metrics(mnames, val_samples, val_response);
    cv_results += metrics_values;

  }

  cv_results /= double(num_folds);
  return cv_results;

}

/// K-folds cross validation (from Dakota)
/* DTS: HAS NOT BEEN TESTED */
RealMatrix Surrogate::cross_validate(const RealMatrix &samples,
    const RealMatrix &response, const StringArray &mnames,
    const int num_folds, const int seed) {

  const int num_metrics = mnames.size();
  const int num_samples = samples.numRows();
  const int num_features = samples.numCols();

  /* Assuming numQOI = 1 */
  Eigen::Map<MatrixXd> eigen_samples(samples.values(), num_samples, num_features);
  Eigen::Map<MatrixXd> eigen_response(response.values(), num_samples, 1);
  VectorXd eigen_cv_results = cross_validate(eigen_samples, eigen_response,
      mnames, num_folds, seed);

  RealMatrix cv_results(Teuchos::Copy, eigen_cv_results.data(),
    num_metrics, num_metrics, 1);

  return cv_results;
}

} // namespace surrogates
} // namespace dakota
