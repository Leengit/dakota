/*  _______________________________________________________________________

    DAKOTA: Design Analysis Kit for Optimization and Terascale Applications
    Copyright 2014 Sandia Corporation.
    This software is distributed under the GNU Lesser General Public License.
    For more information, see the README file in the top Dakota directory.
    _______________________________________________________________________ */


#include "dakota_system_defs.hpp"
#include "dakota_data_types.hpp"
#include "dakota_global_defs.hpp"

#include <boost/assign/list_of.hpp>
#include <boost/algorithm/string.hpp>

#include "H5Cpp.h"      // C++ API
#include "hdf5.h"       // C API
#include "hdf5_hl.h"    // C H5Lite API

#include <iostream>
#include <limits>
#include <memory>
#include <cmath>
#include <string>
#include <vector>

#include "HDF5_IO.hpp"

namespace Dakota
{

//----------------------------------------------------------------

	// Some free functions to try to consolidate data type specs
	inline H5::DataType h5_dtype( const Real & )
	{ return H5::PredType::IEEE_F64LE; }

	inline H5::DataType h5_dtype( const int & )
	{ return H5::PredType::IEEE_F64LE; }

	inline H5::DataType h5_dtype( const char * )
	{
		H5::StrType str_type(0, H5T_VARIABLE);
		str_type.setCset(H5T_CSET_UTF8);  // set character encoding to UTF-8
		return str_type;
	}

	void HDF5IOHelper::attach_scale(
        const String& dset_name, const String& scale_name,
        const String& label, const int& dim) const
    {
		create_groups(dset_name);

        Cerr << "DEBUG Opening scale dataset " << scale_name << std::endl;
        H5::DataSet scale_ds(filePtr->openDataSet(scale_name));
        Cerr << "DEBUG Opening results dataset " << dset_name << std::endl;
        H5::DataSet ds(filePtr->openDataSet(dset_name));
        Cerr << "DEBUG Checking scale status of " << scale_name << std::endl;
        if(!is_scale(scale_ds)) {
            Cerr << "DEBUG Setting scale status for " << scale_name
                << " using label " << label << std::endl;
            H5DSset_scale(scale_ds.getId(), label.c_str() );
            Cerr << "DEBUG Set scale passed for " << scale_name << std::endl;
        }
        Cerr << "DEBUG Attaching scale: " << scale_name << " to " << dset_name << std::endl;
        H5DSattach_scale(ds.getId(), scale_ds.getId(), dim );
        Cerr << "DEBUG Scale attached." << std::endl;
    }


	bool HDF5IOHelper::exists(const String location_name) const
	{
        Cerr << "DEBUG exists() called with " << location_name << std::endl;
		// the first group will be empty due to leading delimiter
		// the last group will be the dataset name
		std::vector<std::string> objects;
		boost::split(objects, location_name, boost::is_any_of("/"));

		// index instead of pruning first or clever iterators
		std::string full_path;
		for( size_t i=1; i<objects.size(); ++i ) {
			full_path += '/' + objects[i];
			// if doesn't exist, add
			if(!filePtr->exists(full_path.c_str())) {
				Cerr << "DEBUG Does not exist: " << full_path << std::endl;
				return false;
			}
		}
		Cerr << "DEBUG existence verified for " << location_name << std::endl;
		return true;
	}

	// Assume we have an absolute path /root/dir/dataset and create
	// groups /root/ and /root/dir/ if needed.
	inline void HDF5IOHelper::create_groups(const std::string& dset_name) const
	{
		// the first group will be empty due to leading delimiter
		// the last group will be the dataset name
		std::vector<std::string> groups;
		boost::split(groups, dset_name, boost::is_any_of("/"));

		// index instead of pruning first and last or clever iterators
		std::string full_path;
		for( size_t i=1; i<(groups.size()-1); ++i ) {
			full_path += '/' + groups[i];
			// if doesn't exist, add

			bool grpexists = filePtr->exists(full_path.c_str());
			if( !grpexists ) {
				filePtr->createGroup(full_path.c_str(), linkCreatePl);
	            /* Add Exception handling
    	        if (create_status < 0)
        	    {
            	  Cerr << "\nError: Could not create group hierarchy \"" << full_path << "\""
                	   << " for HDF5 file \"" << fileName << "\"."
	                   << std::endl;
    	          abort_handler(-1);
        	    }
            	*/
			}
		}
	}

	inline H5::DataSet HDF5IOHelper::create_dataset(
		const H5::H5Location &loc, const std::string &name,
		const H5::DataType &type, const H5::DataSpace &space,
		H5::DSetCreatPropList plist) const {

		hid_t loc_id   = loc.getId();
		hid_t dtype_id = type.getId();
		hid_t space_id = space.getId();
		hid_t lcpl_id  = linkCreatePl.getId();
		hid_t dcpl_id  = plist.getId();

		H5::DataSet dataset(
			H5Dcreate2(loc_id, name.c_str(), dtype_id, space_id, lcpl_id, dcpl_id, H5P_DEFAULT)
		);
		return dataset;
	}

	bool HDF5IOHelper::is_scale(const H5::DataSet dset) const {
		htri_t status = H5DSis_scale(dset.getId());
		if(status > 0) {
			return true;
		} else if(status == 0) {
			return false;
		} else {
			Cerr << "Attempt to determine whether dataset is a scale failed.\n";
			return false;
    	}
    }
}
