cmake_minimum_required(VERSION 3.21)
project(SCUBudgetMVP VERSION 0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets Test)

add_library(budget_core
    src/BudgetModel.cpp
    src/BudgetController.cpp
)
target_include_directories(budget_core PUBLIC src)
target_link_libraries(budget_core PUBLIC Qt6::Core)

add_executable(scu_budget
    src/main.cpp
    src/BudgetView.cpp
)
target_link_libraries(scu_budget PRIVATE budget_core Qt6::Widgets)

enable_testing()
add_executable(budget_tests tests/BudgetTests.cpp src/BudgetView.cpp)
target_link_libraries(budget_tests PRIVATE budget_core Qt6::Test Qt6::Widgets)
add_test(NAME budget_tests COMMAND budget_tests)
set_tests_properties(budget_tests PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
