// This file exists solely to generate main() for the test binary.
// No other test file should ever contain this #define — doctest allows
// only one, and adding it to a second file causes a "duplicate main"
// link error.
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"
