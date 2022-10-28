/*  _______________________________________________________________________

    DAKOTA: Design Analysis Kit for Optimization and Terascale Applications
    Copyright 2014-2022
    National Technology & Engineering Solutions of Sandia, LLC (NTESS).
    This software is distributed under the GNU Lesser General Public License.
    For more information, see the README file in the top Dakota directory.
    _______________________________________________________________________ */

#ifndef _DAKOTA_RESTART_VERSION_H_
#define _DAKOTA_RESTART_VERSION_H_

#include <boost/serialization/access.hpp>
#include <boost/serialization/version.hpp>
#include <limits>
#include <string>

namespace Dakota {

class RestartVersion {

public:

  /// allow boost access to serialize this class
  friend class boost::serialization::access;

  // Informational attributes

  /// the first (lowest) restart version number used in a file
  /** RATIONALE: This is selected to be larger than the first
      serialized short (a Variables view.first) in a historical Dakota
      restart file, so we can detect older files. This embedded
      restart first version number corresponds to < 6.17.0 */
  static const unsigned int restartFirstVersionNumber =
		  std::numeric_limits<short>::max() + 1;

  /// The user-friendly restart version that gets added to first
  /// version number when putting version in a file.
  /** Increment this to increment the actual restart version when one
      or more underlying (or this RestartVersion) class version
      changes. This is also the version of this class for Boost
      purposes. Dakota 6.17.0 ==> versionDelta = 1 */
  static const unsigned int latestRestartVersionDelta = 0;

  /// The latest restart version (that supported by the current source code)
  static const unsigned int latestRestartVersion =
    restartFirstVersionNumber + latestRestartVersionDelta;

  /// first Dakota version to have restart version checking; for
  /// diagnostic messages
  static const std::string firstSupportedDakotaVersion() { return "6.17.0"; }


  /// default ctor used for reading a RestartVersion
  RestartVersion() { /* empty ctor */ };

  /// constructor used for emitting restart version information to file
  RestartVersion(const std::string& dakota_release_ver,
		 const std::string& dakota_release_sha1):
    restartVersion(latestRestartVersion),
    dakotaRelease(dakota_release_ver), dakotaSHA1(dakota_release_sha1)
  { /* empty ctor */ }
	
  /// the user-friendly restart version represented in *this
  unsigned int friendly_rst_version() const
  {
    return (restartVersion < restartFirstVersionNumber) ? 0 :
      restartVersion - restartFirstVersionNumber;
  }

  /// check the read rst_filename's version and issue diagnostic info
  /// vs. current Dakota version
  static RestartVersion check_restart_version(const std::string& rst_filename);


  // Member data corresponding to that stored in a restart file
  
  /// the restart version being processed (defaults to 0 for unknown)
  /** This will be < restartFirstVersionNumber for pre-6.17.0 files
      and >= restartFirstVersionNumber starting with 6.17.0. */
  unsigned int restartVersion = 0;

  /// full release version with any stable or RC append
  std::string dakotaRelease = "<unknown>";
  /// repository revision info
  std::string dakotaSHA1 = "<unknown>";

  /// Boost::serialization serializer
  template<class Archive>
  void serialize(Archive &ar, const unsigned int version) {
    // pre-6.17.0 files will read a short from a prp
    ar & restartVersion;
    // pre-6.17.0 files don't have version information, and certainly
    // not these members:
    if (restartVersion >= restartFirstVersionNumber) {
      ar & dakotaRelease;
      ar & dakotaSHA1;
    }
  }

};

}


BOOST_CLASS_VERSION(Dakota::RestartVersion,
		    Dakota::RestartVersion::latestRestartVersionDelta)

#endif