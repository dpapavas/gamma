include(FindPackageHandleStandardArgs)

find_path(CHIBI_LIB_DIR init-7.scm PATH_SUFFIXES lib/)
find_path(CHIBI_INCLUDE_DIR chibi/sexp.h)
find_library(CHIBI_LIBRARIES libchibi-scheme.a chibi-scheme)

find_package_handle_standard_args(
  Chibi DEFAULT_MSG CHIBI_LIBRARIES CHIBI_INCLUDE_DIR CHIBI_LIB_DIR)
mark_as_advanced(CHIBI_INCLUDE_DIR CHIBI_LIB_DIR CHIBI_LIBRARY)
