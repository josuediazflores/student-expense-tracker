cmake_minimum_required(VERSION 3.16)

project(StudentExpenseTracker)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

# Qt6::Sql gives you QSqlDatabase/QSqlQuery, backed by SQLite.
find_package(Qt6 COMPONENTS Core Widgets Sql REQUIRED)

file(GLOB APP_SOURCES CONFIGURE_DEPENDS src/*.cpp)

# The GUI application.
add_executable(app ${APP_SOURCES})
target_link_libraries(app PRIVATE Qt6::Widgets Qt6::Sql)
target_include_directories(app PRIVATE src)

include(CTest)
if(BUILD_TESTING)
    # Teammates can add test files without defining another main().
    file(GLOB TEST_SOURCES CONFIGURE_DEPENDS tests/*.cpp)
    add_executable(test_build ${TEST_SOURCES} src/BudgetModel.cpp)
    target_link_libraries(test_build PRIVATE Qt6::Core)
    target_include_directories(test_build PRIVATE src tests)
    add_test(NAME test_build COMMAND test_build)
endif()
