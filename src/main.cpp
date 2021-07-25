#include <iostream>
#include <CGAL/assertions.h>

#include "options.h"

int main(int argc, char *argv[])
{
    // Instruct CGAL to throw exceptions, so we can handle them as
    // we please.

    CGAL::set_error_behaviour(CGAL::THROW_EXCEPTION);
    CGAL::set_warning_behaviour(CGAL::THROW_EXCEPTION);

    // Parse the command line.

    return parse_options(argc, argv) < 0 ? EXIT_FAILURE : EXIT_SUCCESS;
}
