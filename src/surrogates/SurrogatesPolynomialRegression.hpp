/*  _______________________________________________________________________

    DAKOTA: Design Analysis Kit for Optimization and Terascale Applications
    Copyright 2014-2020 National Technology & Engineering Solutions of Sandia, LLC (NTESS).
    This software is distributed under the GNU Lesser General Public License.
    For more information, see the README file in the top Dakota directory.
    _______________________________________________________________________ */

#ifndef DAKOTA_SURROGATES_POLYNOMIAL_REGRESSION_HPP
#define DAKOTA_SURROGATES_POLYNOMIAL_REGRESSION_HPP

#include "UtilDataScaler.hpp"
#include "UtilLinearSolvers.hpp"
#include "SurrogatesBase.hpp"
#include "util_data_types.hpp"

#include <boost/serialization/base_object.hpp>

namespace dakota {
namespace surrogates {
/**
 *  \brief The PolynomialRegression class constructs
 *  a polynomial regressor using ordinary least squares.
 *
 *  Users may specify the max degree and p-norm for a
 *  hyperbolic cross scheme to specify the terms in the polynomial
 *  basis. A p-norm = 1 results in a total order specification of
 *  max degree. 
 *
 *  The DataScaler class provides the option of scaling the basis 
 *  matrix.
 */

class PolynomialRegression: public Surrogate {

public:

  /// Constructor that uses defaultConfigOptions and does not build.
  PolynomialRegression();

  /**
   * \brief Constructor that sets configOptions and does not build.
   *
   * \param[in] options List that overrides entries in defaultConfigOptions.
   */
  PolynomialRegression(const ParameterList &options);

  /**
   * \brief Constructor for the PolynomialRegression class that sets configOptions
   *        but does not build the surrogate.
   *
   * \param[in] param_list_xml_filename A ParameterList file (relative to the location of the
   * Dakota input file) that overrides entries in defaultConfigOptions.
   */
  PolynomialRegression(const std::string &param_list_xml_filename); 

  /**
   * \brief Constructor sets configOptions and builds the Polynomial Regression
   *        surrogate.
   *
   * \param[in] samples Matrix of data for surrogate construction - (num_samples by num_features)
   * \param[in] response Vector of targets for surrogate construction - (num_samples by num_qoi = 1; only 1 response is supported currently).
   * \param[in] options List that overrides entries in defaultConfigOptions
   */
  PolynomialRegression(const MatrixXd &samples, const MatrixXd &response,
		                   const ParameterList &options);

  /**
   * \brief Constructor for the PolynomialRegression class that sets configOptions
   *        and builds the surrogate.
   *
   * \param[in] samples Matrix of data for surrogate construction - (num_samples by num_features)
   * \param[in] response Vector of targets for surrogate construction - (num_samples by num_qoi = 1;
   *  only 1 response is supported currently).
   * \param[in] param_list_xml_filename A ParameterList file (relative to the location of the
   *  Dakota input file) that overrides entries in defaultConfigOptions.
   */
  PolynomialRegression(const MatrixXd &samples, const MatrixXd &response,
                       const std::string &param_list_xml_filename);  

  /// Default destructor
  ~PolynomialRegression();


  /**
   * \brief Constructs a basis matrix for a set of samples according to the member 
   *        variable basisIndices.
   *
   * \param[in] samples Matrix of sample points - (num_points by num_features).
   * \param[out] basis_matrix Matrix that contains polynomial basis function evaluations
   * in its rows for each sample point - (num_points by numTerms), numTerms being the number
   * of terms in the polynomial basis.
   */
  void compute_basis_matrix(const MatrixXd &samples, MatrixXd &basis_matrix) const;
  
  /**
   * \brief Build the polynomial surrogate using specified build data.
   *
   * \param[in] samples Matrix of data for surrogate construction - (num_samples by num_features)
   * \param[in] response Vector of targets for surrogate construction - (num_samples by num_qoi = 1; only 1 response is supported currently).
   */
  void build(const MatrixXd &samples, const MatrixXd &response) override;

  /**
   *  \brief Evaluate the polynomial surrogate at a set of prediction points.
   *  \param[in] eval_points Matrix of prediction points - (num_pts by num_features).
   *  \param[out] approx_values Values of the polynomial surrogate at the prediction
   *  points - (num_pts by num_qoi = 1) 
   */
  void value(const MatrixXd &eval_points, MatrixXd &approx_values) override;

  /**
   *  \brief Evaluate the gradient of the polynomial surrogate at a set of prediction points.
   *  \param[in] samples Coordinates of the prediction points - (num_pts by num_features).
   *  \param[out] gradient Matrix of gradient vectors at the prediction points - 
   *  (num_pts by num_features).
   *  \param[out] qoi Index of response/QOI for which to compute derivatives
   */
  void gradient(const MatrixXd &samples, MatrixXd &gradient, const int qoi = 0) override;

  /**
   *  \brief Evaluate the Hessian of the polynomial surrogate at a single point.
   *  \param[in] sample Coordinates of the prediction point - (1 by num_features).
   *  \param[out] hessian Hessian matrix at the prediction point - 
   *  (num_features by num_features).
   *  \param[out] qoi Index of response/QOI for which to compute derivatives
   */
  void hessian(const MatrixXd &sample, MatrixXd &hessian, const int qoi = 0) override;

  /* Getters */

  /// Get the polynomial surrogate's coefficients.
  const MatrixXd& get_polynomial_coeffs() const;
  /// Get the polynomial surrogate's intercept/offset.
  double get_polynomial_intercept() const;
  /// Get the number of terms in the polynomial surrogate.
  int get_num_terms() const;

  /* Setters */
  /// Set the polynomial surrogate's coefficients.
  void set_polynomial_coeffs(const MatrixXd &coeffs);


  std::shared_ptr<Surrogate> clone() const override
  { return std::make_shared<PolynomialRegression>(configOptions); }

private:
  /// Construct and populate the defaultConfigOptions.
  void default_options() override;

  /// Matrix that specifies the powers of each variable for each term
  /// in the polynomial - (numVariables by numTerms).
  MatrixXi basisIndices;
  /// Linear solver for the ordinary least squares problem.
  std::shared_ptr<util::LinearSolverBase> linearSolver;

  /// Number of terms in the polynomial basis.
  int numTerms;
  /// Vector of coefficients for the polynomial surrogate.
  MatrixXd polynomialCoeffs;
  /// Offset/intercept term for the polynomial surrogate.
  double polynomialIntercept;

  /// Allow serializers access to private class data
  friend class boost::serialization::access;
  /// Serializer for save/load
  template<class Archive>
  void serialize(Archive& archive, const unsigned int version);
};


template<class Archive>
void PolynomialRegression::serialize(Archive& archive, const unsigned int version)
{
  archive & boost::serialization::base_object<Surrogate>(*this);
  archive & numTerms;
  archive & basisIndices;
  archive & polynomialCoeffs;
  archive & polynomialIntercept;
}


} // namespace surrogates
} // namespace dakota

#endif
