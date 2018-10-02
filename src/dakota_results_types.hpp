/*  _______________________________________________________________________

    DAKOTA: Design Analysis Kit for Optimization and Terascale Applications
    Copyright 2014 Sandia Corporation.
    This software is distributed under the GNU Lesser General Public License.
    For more information, see the README file in the top Dakota directory.
    _______________________________________________________________________ */

// Results database supporting definitions
//  * Data type definitions for keys and meta data
//  * Free helper functions to create and manage data entries

#ifndef DAKOTA_RESULTS_TYPES_H
#define DAKOTA_RESULTS_TYPES_H

#include <map>
#include <vector>
#include <algorithm>
#include <boost/tuple/tuple.hpp>
#include "dakota_data_types.hpp"
#include <boost/any.hpp>
#include <boost/variant.hpp>

namespace Dakota {

// TODO: Using a tokenized string "instance::execution::result_key" or
// a list of keys might be more flexible going forward, instead of tuple.

// -----------
// Results Key
// -----------

/// Data type for results key (instance name / id, unique run, label),
/// where data_key is a valid colon-delimited string from ResultsNames 
/// tuple<method_name, method_id, execution_number, data_key>
typedef 
boost::tuple<std::string, std::string, size_t, std::string> ResultsKeyType;


// -----------
// Results Data
// -----------

// NOTE: the ResultsDataType varies by underlying database


// ----------------
// Results Metadata
// ----------------

/// Data type for metadata key
typedef std::string MetaDataKeyType;

/// Data type for metadata value
typedef std::vector<std::string> MetaDataValueType;

/// A single MetaData entry is map<string, vector<string> >  Example:
///   pair( "Column labels", ["Mean", "Std Dev", "Skewness", "Kurtosis"] )
typedef std::map<MetaDataKeyType, MetaDataValueType> MetaDataType;


// --------------
// Other Typedefs
// --------------

// TODO: This can probably go away
//typedef std::pair<std::string, size_t> StringSizetPair;

/// Iterator unique ID: <method_name, method_id, exec_num>
typedef boost::tuple<std::string, std::string, size_t> StrStrSizet;

/// Make a full ResultsKeyType from the passed iterator_id and data_name
ResultsKeyType make_key(const StrStrSizet& iterator_id, 
			const std::string& data_name);

/// create MetaDataValueType from the passed strings
MetaDataValueType 
make_metadatavalue(StringMultiArrayConstView labels);

/// create MetaDataValueType from the passed strings
MetaDataValueType 
make_metadatavalue(StringMultiArrayConstView cv_labels,
		   StringMultiArrayConstView div_labels,
		   StringMultiArrayConstView drv_labels,
		   const StringArray& resp_labels);

/// create MetaDataValueType from the passed strings
MetaDataValueType 
make_metadatavalue(const StringArray& resp_labels);

/// create MetaDataValueType from the passed strings
MetaDataValueType
make_metadatavalue(const std::string&);

/// create MetaDataValueType from the passed strings
MetaDataValueType
make_metadatavalue(const std::string&, const std::string&);

/// create MetaDataValueType from the passed strings
MetaDataValueType
make_metadatavalue(const std::string&, const std::string&, 
		   const std::string&);

/// create MetaDataValueType from the passed strings
MetaDataValueType
make_metadatavalue(const std::string&, const std::string&,
		   const std::string&, const std::string&);



// Inline definitions of helper functions

inline MetaDataValueType 
make_metadatavalue(StringMultiArrayConstView labels)
{
  MetaDataValueType mdv;
  for (size_t i=0; i<labels.size(); ++i)
    mdv.push_back(labels[i]);
  return mdv;
}


inline MetaDataValueType 
make_metadatavalue(StringMultiArrayConstView cv_labels,
		   StringMultiArrayConstView div_labels,
		   StringMultiArrayConstView dsv_labels,
		   StringMultiArrayConstView drv_labels,
		   const StringArray& resp_labels)
{
  // could use iterators for some of these
  MetaDataValueType mdv;
  for (size_t i=0; i<cv_labels.size(); ++i)
    mdv.push_back(cv_labels[i]);
  for (size_t i=0; i<div_labels.size(); ++i)
    mdv.push_back(div_labels[i]);
  for (size_t i=0; i<dsv_labels.size(); ++i)
    mdv.push_back(dsv_labels[i]);
  for (size_t i=0; i<drv_labels.size(); ++i)
    mdv.push_back(drv_labels[i]);
  for (size_t i=0; i<resp_labels.size(); ++i)
    mdv.push_back(resp_labels[i]);
  
  return mdv;
}

inline MetaDataValueType 
make_metadatavalue(const StringArray& resp_labels)
{
  // could use iterators for some of these
  MetaDataValueType mdv;
  for (size_t i=0; i<resp_labels.size(); ++i)
    mdv.push_back(resp_labels[i]);
  
  return mdv;
}

inline MetaDataValueType
make_metadatavalue(const std::string& s1)
{
  MetaDataValueType mdv;
  mdv.push_back(s1);
  return mdv;
}

inline MetaDataValueType
make_metadatavalue(const std::string& s1, const std::string& s2)
{
  MetaDataValueType mdv;
  mdv.push_back(s1);
  mdv.push_back(s2);
  return mdv;
}

inline MetaDataValueType
make_metadatavalue(const std::string& s1, const std::string& s2, 
		   const std::string& s3)
{
  MetaDataValueType mdv;
  mdv.push_back(s1);
  mdv.push_back(s2);
  mdv.push_back(s3);
  return mdv;
}

inline MetaDataValueType
make_metadatavalue(const std::string& s1, const std::string& s2,
		   const std::string& s3, const std::string& s4)
{
  MetaDataValueType mdv;
  mdv.push_back(s1);
  mdv.push_back(s2);
  mdv.push_back(s3);
  mdv.push_back(s4);
  return mdv;
}

inline ResultsKeyType make_key(const StrStrSizet& iterator_id, 
			       const std::string& data_name)
{
  return ResultsKeyType(iterator_id.get<0>(), 
			iterator_id.get<1>(), 
			iterator_id.get<2>(),
			data_name);
}

// The following structs and typedefs are used to insert
// results into the results database using the
// ResultsManager interface.
//
// The first set of items are for specifying dimension
// scales, which are labels (strings or reals) for each
// axis of a set of data (vector or matrix, currently).
//
// Dimension scales are of type multimap<int, boost::variant>
// (typedef'ed to DimScaleMap), with the integer indicating the 
// dimenions of the dataset that the scale is associated with, 
// and the boost::variant containing the scale
// itself, either a RealScale or a StringScale. These latter
// types are structs that contain a label, which is like a 
// heading for the scale, the items in the scale, and a 
// ScaleScope enum, which determines whether the scale is 
// SHARED among multiple responses or is unique to a particular 
// response (UNHSARED).

/// Enum to specify whether a scale shared among responses
enum class ScaleScope {SHARED, UNSHARED};

// RealScale and StringScale avoid making copies of data to
// save memory, which may be important for large results.
// For HDF5, the scales are immediately written to disk, and so
// it's not necessary for the client to preserve objects in
// memory after using a RealScale or StringScale in a call
// to ResultsManager.insert(...).

/// Data structure for storing real-valued dimension scale
struct RealScale {

  /// Constructor that takes a RealVector 
  RealScale(const std::string &label, const RealVector &in_items, 
          ScaleScope scope = ScaleScope::UNSHARED) : 
          label(label), scope(scope) {
            items = RealVector(Teuchos::View, 
                *const_cast<RealVector*>(&in_items));
            numCols = items.length();
            isMatrix = false;
          }

  /// Constructor that takes a RealArray
  RealScale(const std::string &label, const RealArray &in_items, 
          ScaleScope scope = ScaleScope::UNSHARED) : 
          label(label), scope(scope) {
            items = RealVector(Teuchos::View, 
                const_cast<Real*>(in_items.data()),
                in_items.size());
            numCols = items.length();
            isMatrix = false;
          }

  // Constructor that takes a C-style array
  RealScale(const std::string &label, const Real in_items[], 
          ScaleScope scope = ScaleScope::UNSHARED) : 
          label(label), scope(scope) {
            int len = sizeof(in_items)/sizeof(in_items[0]);
            items = RealVector(Teuchos::View,
                const_cast<Real*>(in_items), len);
            numCols = items.length();
            isMatrix = false;
          }

  // Constructor that takes a pointer to Real and length
  RealScale(const std::string &label, const Real *in_items, const int len,
          ScaleScope scope = ScaleScope::UNSHARED) : 
          label(label), scope(scope) {
            items = RealVector(Teuchos::View,
                const_cast<Real*>(in_items), len);
            numCols = items.length();
            isMatrix = false;
          }

  // Name of the scale
  std::string label;
  // Scope of the scale (whether it is shared among responses)
  ScaleScope scope;
  // Items in the scale
  RealVector items;
  /// Number of columns; equals length of scale when 1D
  int numCols;
  /// 2d or 1d?
  bool isMatrix;
};


/// Data structure for storing string-valued dimension scale
struct StringScale {
  /// Constructor that takes a C-style array of C-strings
  StringScale(const std::string& in_label, const char * const in_items[], 
          ScaleScope in_scope = ScaleScope::UNSHARED) {
    label = in_label;
    int len = sizeof(in_items)/sizeof(in_items[0]);
    items.resize(len);
    std::copy(in_items, in_items + len, items.begin());
    scope = in_scope;
    numCols = len;
    isMatrix = false;
  }

  /// Constructor that takes and initializer list of string literals
  StringScale(const std::string & in_label, 
        std::initializer_list<const char *> in_items,
        ScaleScope in_scope = ScaleScope::UNSHARED) {
    label = in_label;
    items.resize(in_items.size());
    std::copy(in_items.begin(), in_items.end(), items.begin()); 
    scope = in_scope;
    numCols = items.size();
    isMatrix = false;
  }

  /// Constructor that takes a vector of strings
  StringScale(const std::string& in_label, const std::vector<String> &in_items, 
          ScaleScope in_scope = ScaleScope::UNSHARED) {
    label = in_label;
    scope = in_scope;
    items.resize(in_items.size());
    std::transform(in_items.begin(), in_items.end(), items.begin(), 
      [](const String &s) { return s.c_str();});
    numCols = items.size();
    isMatrix = false;
  }

  /// Constructor that takes a vector of C-style strings
  StringScale(const std::string & in_label, 
        std::vector<const char *> in_items,
        ScaleScope in_scope = ScaleScope::UNSHARED) {
    label = in_label;
    items.resize(in_items.size());
    std::copy(in_items.begin(), in_items.end(), items.begin()); 
    scope = in_scope;
    numCols = items.size();
    isMatrix = false;
  }

  /// Constructor that takes a vector<vector<const char *> > to produce a 2D scale
  StringScale(const std::string & in_label,
      std::vector<std::vector<const char *> > in_items,
      ScaleScope in_scope = ScaleScope::UNSHARED) {
    label = in_label;
    int num_rows = in_items.size();
    numCols = in_items[0].size(); // TODO: error checking to confirm a "square matrix"
    items.resize(num_rows*numCols);
    int offset = 0;
    for(auto &v : in_items) {
      std::copy(v.begin(), v.end(), items.begin() + offset);
      offset += numCols;
    }
    scope = in_scope;
    isMatrix = true;
  }

  /// Scale label
  std::string label;
  /// Scale scope (whether the scaled is shared among responses)
  ScaleScope scope;
  /// Pointers to the strings that make up the scale
  std::vector<const char *> items;
  /// Number of columns; equals length of scale when 1D
  int numCols;
  /// 2d or 1d?
  bool isMatrix;
};

/// Datatype to communicate scales (stored in boost::variant) and their
/// associated dimension (the int) to the ResultsManager instance.
typedef std::multimap<int, boost::variant<StringScale, RealScale> > DimScaleMap;


// HDF5 objects can have key:value metadata attached to them. These
// are called attributes. We support integer, real, and string valued 
// metadata  using these type defintions.

/// Data structure for a single Real, String, or int valued attribute
template <typename T>
struct ResultAttribute {
  /// Construct an attribute 
  ResultAttribute(const String &label, const T &value) :
    label(label), value(value) {};
  /// Key for the attribute
  String label;
  /// Value for the attribute
  T value;
};

/// Datatype to communcate metadata (attributes) to the ResultsManager
/// instance.
typedef std::vector<boost::variant< ResultAttribute<int>, 
                                    ResultAttribute<String>,
                                    ResultAttribute<Real> > > AttributeArray;


}  // namespace Dakota

#endif  // DAKOTA_RESULTS_TYPES_H
