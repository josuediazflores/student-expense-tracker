cmake_minimum_required(VERSION 3.16)

project(StudentExpenseTracker)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

# Qt6::Sql gives you QSqlDatabase/QSqlQuery, backed by SQLite.
find_package(Qt6 COMPONENTS Widgets Sql REQUIRED)

# Collect all source files in src/ and all test files in tests/.
# You do NOT need to edit this file when you add a new .cpp file.
# Just create the file in src/ or tests/ and it will be picked up.
file(GLOB APP_SOURCES  CONFIGURE_DEPENDS src/*.cpp)
file(GLOB TEST_SOURCES CONFIGURE_DEPENDS tests/*.cpp)

# The test binary must not include main.cpp, or you get a
# "duplicate main" link error (tests/test_main.cpp provides main()).
set(APP_SOURCES_NO_MAIN ${APP_SOURCES})
list(REMOVE_ITEM APP_SOURCES_NO_MAIN ${CMAKE_CURRENT_SOURCE_DIR}/src/main.cpp)

# The GUI application.
add_executable(app ${APP_SOURCES})
target_link_libraries(app PRIVATE Qt6::Widgets Qt6::Sql)
target_include_directories(app PRIVATE src)

# The test binary: every test file, plus your source files except main.cpp.
# In this class we only write tests for model classes, not view or
# controller files. Copy tests/test_prime.cpp as a starting point for a
# new test file — no special #define needed at the top.
add_executable(test_build ${TEST_SOURCES} ${APP_SOURCES_NO_MAIN})
target_link_libraries(test_build PRIVATE Qt6::Widgets Qt6::Sql)
target_include_directories(test_build PRIVATE src tests)

enable_testing()
add_test(NAME test_build COMMAND test_build)
