/*  _______________________________________________________________________

    DAKOTA: Design Analysis Kit for Optimization and Terascale Applications
    Copyright 2014 Sandia Corporation.
    This software is distributed under the GNU Lesser General Public License.
    For more information, see the README file in the top Dakota directory.
    _______________________________________________________________________ */


#ifdef DAKOTA_HAVE_HDF5

//#include <limits>
#include <string>
//#include <map>

#include "dakota_data_types.hpp"
#include "dakota_global_defs.hpp"
#include "BinaryIO_Helper.hpp"

#include <Teuchos_UnitTestHarness.hpp> 
#include <Teuchos_SerialDenseHelpers.hpp>
#include "H5Cpp.h"
#include "HDF5_IO.hpp"

#ifndef H5_NO_NAMESPACE
     using namespace H5;
#ifndef H5_NO_STD
     using std::cout;
     using std::endl;
#endif  // H5_NO_STD
#endif

const H5std_string	FILE_NAME("h5tutr_subset.h5");
const H5std_string	DATASET_NAME("IntArray");

using namespace Dakota;

//----------------------------------------------------------------

namespace {


  template<typename T>
  double
  max_diff( const Teuchos::SerialDenseVector<int,T> & vec1,
            const Teuchos::SerialDenseVector<int,T> & vec2 )
  {
    double diff_val = 0.0;
    double diff_max = 0.0;

    for( int i=0; i<vec1.length(); ++i )
    {
      diff_val = std::abs(vec1[i] - vec2[i]);
      if( diff_max < diff_val )
        diff_max = diff_val;
    }

    return diff_max;
  }

  const size_t VEC_SIZE = 5;
  const size_t MAT_ROWS = 3;
  const size_t MAT_COLS = 4;

} // anonymous namespace

//----------------------------------------------------------------



TEUCHOS_UNIT_TEST(hdf5_cpp, scalar)
{
  const std::string file_name("hdf5_scalar.h5");
  const std::string ds_name("/SomeScalarData");

  Real value = 1.23;

  // Write data
  {
    HDF5IOHelper h5_io(file_name, /* overwrite */ true);
    h5_io.store_scalar(ds_name, value);
  }

  // Read data
  {
    HDF5IOHelper h5_io(file_name);
    h5_io.read_scalar(ds_name, value);
  }

  TEST_ASSERT( true );
}

//----------------------------------------------------------------

TEUCHOS_UNIT_TEST(hdf5_cpp, realvec)
{
  const std::string file_name("hdf5_basic_realvec.h5");
  const std::string ds_name("/Level_1/level_2/SomeRealVectorData");

  RealVector vec_out(VEC_SIZE);
  vec_out.random();

  // Write data
  {
    HDF5IOHelper h5_io(file_name, /* overwrite */ true);
    h5_io.store_vector(ds_name, vec_out);
  }

  // Read data
  RealVector test_vec(VEC_SIZE);
  {
    HDF5IOHelper h5_io(file_name);
    h5_io.read_vector(ds_name, test_vec);
  }

  TEST_EQUALITY( test_vec.length(), vec_out.length() );
  double diff = max_diff( vec_out, test_vec );
  TEST_COMPARE( diff, <, 1.e-15 );
}

//----------------------------------------------------------------

TEUCHOS_UNIT_TEST(hdf5_cpp, stdvec)
{
  const std::string file_name("hdf5_basic_stdvec.h5");
  const std::string ds_name("/Level_One/level_two/2/b/SomeStdVectorData");

  RealVector rvec(VEC_SIZE);
  rvec.random();

  std::vector<Real> vec_out(VEC_SIZE);
  int i = 0;
  for( auto & e : vec_out ) 
    e = rvec[i++];

  // Write data
  {
    HDF5IOHelper h5_io(file_name, /* overwrite */ true);
    h5_io.store_vector(ds_name, vec_out);
  }

  // Read data
  std::vector<Real> test_vec(VEC_SIZE);
  {
    HDF5IOHelper h5_io(file_name);
    h5_io.read_vector(ds_name, test_vec);
  }

  TEST_EQUALITY( test_vec.size(), vec_out.size() );
  TEST_COMPARE_ARRAYS( vec_out, test_vec );
}

TEUCHOS_UNIT_TEST(hdf5_cpp, element_store)
{
  const std::string file_name("hdf5_element_store.h5");
  const std::string ds_name("/element_ds");

  RealVector rvec(VEC_SIZE);
  rvec.random();
  
    std::vector<Real> vec_out(VEC_SIZE);
  int i = 0;
  for( auto & e : vec_out ) 
    e = rvec[i++];

  // Write data
  {
    HDF5IOHelper h5_io(file_name, /* overwrite */ true);
    std::vector<int> dims = {VEC_SIZE};
    h5_io.create_empty_dataset(ds_name, dims, Dakota::ResultsOutputType::REAL, VEC_SIZE);
    for(int i = 0; i < VEC_SIZE; ++i)
      h5_io.set_scalar(ds_name, vec_out[i], i);
  }

  // Read data
  std::vector<Real> test_vec(VEC_SIZE);
  {
    HDF5IOHelper h5_io(file_name);
    h5_io.read_vector(ds_name, test_vec);
  }

  TEST_EQUALITY( test_vec.size(), vec_out.size() );
  TEST_COMPARE_ARRAYS( vec_out, test_vec );
}

TEUCHOS_UNIT_TEST(hdf5_cpp, element_append)
{
  const std::string file_name("hdf5_element_append.h5");
  const std::string ds_name("/unlimited_ds");

  RealVector rvec(VEC_SIZE);
  rvec.random();
  
    std::vector<Real> vec_out(VEC_SIZE);
  int i = 0;
  for( auto & e : vec_out ) 
    e = rvec[i++];

  // Write data
  {
    HDF5IOHelper h5_io(file_name, /* overwrite */ true);
    std::vector<int> dims = {0};
    h5_io.create_empty_dataset(ds_name, dims, Dakota::ResultsOutputType::REAL, VEC_SIZE);
    for(auto e : vec_out)
      h5_io.append_scalar(ds_name, e);
  }

  // Read data
  std::vector<Real> test_vec(VEC_SIZE);
  {
    HDF5IOHelper h5_io(file_name);
    h5_io.read_vector(ds_name, test_vec);
  }

  TEST_EQUALITY( test_vec.size(), vec_out.size() );
  TEST_COMPARE_ARRAYS( vec_out, test_vec );
}

TEUCHOS_UNIT_TEST(hdf5_cpp, row_append)
{
  const std::string file_name("hdf5_row_append.h5");
  const std::string ds_name("/unlimited_rows");

  // ****  Add rows  ****
  RealMatrix rmat(MAT_ROWS, MAT_COLS);
  rmat.random();

  // Write data
  {
    HDF5IOHelper h5_io(file_name, /* overwrite */ true);
    std::vector<int> dims = {0, MAT_COLS};
    h5_io.create_empty_dataset(ds_name, dims, Dakota::ResultsOutputType::REAL, MAT_ROWS);
    RealVector vec_out(MAT_COLS);
    for(int i = 0; i < MAT_ROWS; ++i) {
      for(int j = 0; j < MAT_COLS; ++j)
        vec_out[j] = rmat(i,j);
      h5_io.append_vector(ds_name, vec_out, true);
    }
  }
  
  // Read data
  
  RealMatrix test_mat_original;
  {
    HDF5IOHelper h5_io(file_name);
    h5_io.read_matrix(ds_name, test_mat_original, false);
  }

  TEST_EQUALITY(test_mat_original.numRows(), MAT_ROWS);
  TEST_EQUALITY(test_mat_original.numCols(), MAT_COLS);
  
  for(int i = 0; i < MAT_ROWS; ++i) {
    for(int j = 0; j < MAT_COLS; ++j) {
      TEST_EQUALITY( rmat(i,j), test_mat_original(i,j) );
    }
  }

  RealMatrix test_mat_transposed;
  {
    HDF5IOHelper h5_io(file_name);
    h5_io.read_matrix(ds_name, test_mat_transposed, true);
  }

  TEST_EQUALITY(test_mat_transposed.numRows(), MAT_COLS);
  TEST_EQUALITY(test_mat_transposed.numCols(), MAT_ROWS);
  
  for(int i = 0; i < MAT_ROWS; ++i) {
    for(int j = 0; j < MAT_COLS; ++j) {
      TEST_EQUALITY( rmat(i,j), test_mat_transposed(j,i) );
    }
  }
}

/* This capability has been disabled for now
  TEUCHOS_UNIT_TEST(hdf5_cpp, col_append)
{
  const std::string file_name("hdf5_col_append.h5");
  const std::string ds_name("/unlimited_cols");

  // ****  Add rows  ****
  RealMatrix rmat(MAT_ROWS, MAT_COLS);
  rmat.random();

  // Write data
  {
    HDF5IOHelper h5_io(file_name, true);
    std::vector<int> dims = {MAT_ROWS, 0};
    h5_io.create_empty_dataset(ds_name, dims, Dakota::ResultsOutputType::REAL, MAT_COLS);
    for(int i = 0; i < MAT_COLS; ++i) {
      RealVector vec_out = Teuchos::getCol<int,Real>(Teuchos::View, rmat, i);
      h5_io.append_vector(ds_name, vec_out, false );
    }
  }
  
  // Read data
  
  RealMatrix test_mat_original;
  {
    HDF5IOHelper h5_io(file_name);
    h5_io.read_matrix(ds_name, test_mat_original, false);
  }

  TEST_EQUALITY(test_mat_original.numRows(), MAT_ROWS);
  TEST_EQUALITY(test_mat_original.numCols(), MAT_COLS);
  
  for(int i = 0; i < MAT_ROWS; ++i) {
    for(int j = 0; j < MAT_COLS; ++j) {
      TEST_EQUALITY( rmat(i,j), test_mat_original(i,j) );
    }
  }

  RealMatrix test_mat_transposed;
  {
    HDF5IOHelper h5_io(file_name);
    h5_io.read_matrix(ds_name, test_mat_transposed, true);
  }

  TEST_EQUALITY(test_mat_transposed.numRows(), MAT_COLS);
  TEST_EQUALITY(test_mat_transposed.numCols(), MAT_ROWS);
  
  for(int i = 0; i < MAT_ROWS; ++i) {
    for(int j = 0; j < MAT_COLS; ++j) {
      TEST_EQUALITY( rmat(i,j), test_mat_transposed(j,i) );
    }
  }
}
*/

TEUCHOS_UNIT_TEST(hdf5_cpp, matrix_append)
{
  const std::string file_name("hdf5_matrix_append.h5");
  const std::string ds_name("/original");

  int num_matrix = 2;
  std::vector<RealMatrix> original_vec(num_matrix);
  for(auto & m : original_vec) {
    m.shapeUninitialized(MAT_ROWS, MAT_COLS);
    m.random();
  }
  // Write data
  {
    HDF5IOHelper h5_io(file_name, /* overwrite */ true);
    std::vector<int> dims = {0, MAT_ROWS, MAT_COLS};
    h5_io.create_empty_dataset(ds_name, dims, Dakota::ResultsOutputType::REAL, num_matrix);
    for(int i = 0; i < num_matrix ; ++i) {
      h5_io.append_matrix(ds_name, original_vec[i], false); 
    }
  }
    
  // Read data tranposed
  std::vector<RealMatrix> original_vec_test(num_matrix);
  {
    HDF5IOHelper h5_io(file_name);
    for(int i = 0; i < num_matrix; ++i)
      h5_io.get_matrix(ds_name, original_vec_test[i], i, true);
  }
   
  for(int i = 0; i < num_matrix; ++i) {
    TEST_EQUALITY(original_vec_test[i].numRows(), MAT_COLS);
    TEST_EQUALITY(original_vec_test[i].numCols(), MAT_ROWS);
  }
  for(int mi = 0; mi < num_matrix; ++mi) {
    for(int i = 0; i < MAT_ROWS; ++i) {
      for(int j = 0; j < MAT_COLS; ++j) {
        TEST_EQUALITY( original_vec[mi](i,j), original_vec_test[mi](j,i) );
      }
    }
  }
  
  // Write data tranposed
  {
    HDF5IOHelper h5_io(file_name, true);
    std::vector<int> dims = {0, MAT_COLS, MAT_ROWS};
    h5_io.create_empty_dataset(ds_name, dims, Dakota::ResultsOutputType::REAL, num_matrix);
    for(int i = 0; i < num_matrix ; ++i) {
      h5_io.append_matrix(ds_name, original_vec[i], true);
    }
  }
    
  // Read data
  {
    HDF5IOHelper h5_io(file_name);
    for(int i = 0; i < num_matrix; ++i) {
      h5_io.get_matrix(ds_name, original_vec_test[i], i, false);
    }
  }
   
  for(int i = 0; i < num_matrix; ++i) {
    TEST_EQUALITY(original_vec_test[i].numRows(), MAT_COLS);
    TEST_EQUALITY(original_vec_test[i].numCols(), MAT_ROWS);
  }
  for(int mi = 0; mi < num_matrix; ++mi) {
    for(int i = 0; i < MAT_ROWS; ++i) {
      for(int j = 0; j < MAT_COLS; ++j) {
        TEST_EQUALITY( original_vec[mi](i,j), original_vec_test[mi](j,i) );
      }
    }
  }
   
}

TEUCHOS_UNIT_TEST(hdf5_cpp, file_modes)
{
  const std::string file_name("hdf5_file_modes.h5");
  const std::string ds_name("/a_scalar");

  // Overwrite (potentially) existing file and write a scalar
  {
    HDF5IOHelper h5_io(file_name, /* overwrite */ true);
    TEST_ASSERT(!h5_io.exists(ds_name));
    h5_io.store_scalar(ds_name, 5);
  }
    
  // Read the scalar (make sure the file created above isn't truncated)
  {
    HDF5IOHelper h5_io(file_name,/* don't overwrite */ false);
    TEST_ASSERT(h5_io.exists(ds_name));
    Real x;
    h5_io.read_scalar(ds_name, x);
    TEST_EQUALITY(x, 5);
  }
  // Overwrite existing file again, which definitely now exists
  {
    HDF5IOHelper h5_io(file_name, /* overwrite */ true);
    TEST_ASSERT(!h5_io.exists(ds_name));
  }
}   

TEUCHOS_UNIT_TEST(hdf5_cpp, vector_matrix_append)
{
  const std::string file_name("hdf5_vector_matrix_append.h5");
  const std::string ds_original_name("/original");
  const std::string ds_transposed_name("/transposed");

  // ****  Add un-transposed  ****
  int num_matrix = 2;
  int num_layer = 2;
  std::vector<std::vector<RealMatrix> > original_vec(num_layer);
  for(auto & layer : original_vec) {
    layer.resize(num_matrix);
    for(auto &m : layer) {
      m.shapeUninitialized(MAT_ROWS, MAT_COLS);
      m.random();
    }
  }
  // Write data
  {
    HDF5IOHelper h5_io(file_name, /* overwrite */ true);
    std::vector<int> dims = {0, num_matrix, MAT_ROWS, MAT_COLS};
    h5_io.create_empty_dataset(ds_original_name, dims, Dakota::ResultsOutputType::REAL, num_layer);
    for(int i = 0; i < num_matrix ; ++i) {
      h5_io.append_vector_matrix(ds_original_name, original_vec[i], false); 
    }
  }
    
  // Read data tranposed
  
  std::vector<std::vector<RealMatrix> > original_vec_test(num_layer);
  {
    HDF5IOHelper h5_io(file_name);
    for(int i = 0; i < num_layer; ++i) 
      h5_io.get_vector_matrix(ds_original_name, original_vec_test[i], i, true);
  }
   
  for(int i = 0; i < num_layer; ++i) {
    for(int j = 0; j < num_matrix; ++j) {
      TEST_EQUALITY(original_vec_test[i][j].numRows(), MAT_COLS);
      TEST_EQUALITY(original_vec_test[i][j].numCols(), MAT_ROWS);
    }
  }
  for(int li = 0; li < num_layer; ++li) {
    for(int mi = 0; mi < num_matrix; ++mi) {
      for(int i = 0; i < MAT_ROWS; ++i) {
        for(int j = 0; j < MAT_COLS; ++j) {
          TEST_EQUALITY( original_vec[li][mi](i,j), original_vec_test[li][mi](j,i) );
        }
      }
    }
  }

  // Write data transposed
  {
    HDF5IOHelper h5_io(file_name, /* overwrite */ true);
    std::vector<int> dims = {0, num_matrix, MAT_COLS, MAT_ROWS};
    h5_io.create_empty_dataset(ds_original_name, dims, Dakota::ResultsOutputType::REAL, num_layer);
    for(int i = 0; i < num_matrix ; ++i) {
      h5_io.append_vector_matrix(ds_original_name, original_vec[i], true); 
    }
  }
  
  // Read tranposed
  {
    HDF5IOHelper h5_io(file_name);
    for(int i = 0; i < num_layer; ++i) 
      h5_io.get_vector_matrix(ds_original_name, original_vec_test[i], i, false);
  }
   
  for(int i = 0; i < num_layer; ++i) {
    for(int j = 0; j < num_matrix; ++j) {
      TEST_EQUALITY(original_vec_test[i][j].numRows(), MAT_COLS);
      TEST_EQUALITY(original_vec_test[i][j].numCols(), MAT_ROWS);
    }
  }
  for(int li = 0; li < num_layer; ++li) {
    for(int mi = 0; mi < num_matrix; ++mi) {
      for(int i = 0; i < MAT_ROWS; ++i) {
        for(int j = 0; j < MAT_COLS; ++j) {
          TEST_EQUALITY( original_vec[li][mi](i,j), original_vec_test[li][mi](j,i) );
        }
      }
    }
  }

 }
 //----------------------------------------------------------------

#endif
