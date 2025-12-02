include(FindPackageHandleStandardArgs)

find_path(GL2PS_INCLUDE_DIR gl2ps.h)
find_library(GL2PS_LIBRARY gl2ps)

find_package_handle_standard_args(
  GL2PS DEFAULT_MSG GL2PS_LIBRARY GL2PS_INCLUDE_DIR)
mark_as_advanced(GL2PS_INCLUDE_DIR GL2PS_LIBRARY)
