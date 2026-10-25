cmake_minimum_required(VERSION 3.16)

project(StudentExpenseTracker)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

# Qt6::Sql gives you QSqlDatabase/QSqlQuery, backed by SQLite.
find_package(Qt6 COMPONENTS Widgets Sql REQUIRED)

file(GLOB APP_SOURCES CONFIGURE_DEPENDS src/*.cpp)

# The GUI application.
add_executable(app ${APP_SOURCES})
target_link_libraries(app PRIVATE Qt6::Widgets Qt6::Sql)
target_include_directories(app PRIVATE src)
