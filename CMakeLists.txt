cmake_minimum_required(VERSION 3.16)

project(StudentExpenseTracker)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

# Qt6::Sql gives you QSqlDatabase/QSqlQuery, backed by SQLite.
find_package(Qt6 COMPONENTS Core Widgets Sql REQUIRED)

# Each MVC layer is its own target, so a Model or Controller that touches a
# Widgets symbol fails to link instead of quietly depending on the GUI.
add_library(budget_model STATIC src/BudgetModel.cpp src/DecimalCents.cpp)
target_include_directories(budget_model PUBLIC src)
target_link_libraries(budget_model PUBLIC Qt6::Core)

# Saves the model to SQLite. It sits beside the Model, so the Model never links Qt6::Sql.
add_library(budget_store STATIC src/BudgetStore.cpp)
target_link_libraries(budget_store PUBLIC budget_model Qt6::Core Qt6::Sql)

add_library(budget_controller STATIC src/BudgetController.cpp)
target_link_libraries(budget_controller PUBLIC budget_model budget_store Qt6::Core)

# The GUI application.
add_executable(app src/main.cpp src/BudgetView.cpp)
target_link_libraries(app PRIVATE budget_controller Qt6::Widgets)

include(CTest)
if(BUILD_TESTING)
    # Teammates can add test files without defining another main().
    file(GLOB TEST_SOURCES CONFIGURE_DEPENDS tests/*.cpp)
    add_executable(test_build ${TEST_SOURCES})
    target_link_libraries(test_build PRIVATE budget_controller Qt6::Core Qt6::Sql)
    target_include_directories(test_build PRIVATE tests)
    add_test(NAME test_build COMMAND test_build)
    add_test(NAME layering
             COMMAND ${CMAKE_COMMAND} -DSOURCE_DIR=${CMAKE_SOURCE_DIR}
                     -P ${CMAKE_SOURCE_DIR}/tests/CheckLayering.cmake)
endif()
