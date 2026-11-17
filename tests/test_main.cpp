// Compile the doctest implementation in this translation unit only.
#define DOCTEST_CONFIG_IMPLEMENT
#include "doctest.h"

#include <QCoreApplication>

int main(int argc, char *argv[]) {
    // Keep Qt alive for all tests and destroy it before static teardown.
    QCoreApplication app(argc, argv);
    doctest::Context tests(argc, argv);
    return tests.run();
}
