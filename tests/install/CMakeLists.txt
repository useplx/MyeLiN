cmake_minimum_required(VERSION 3.21)
project(myelin_install_test LANGUAGES CXX)

find_package(MyeLiN 1.0 REQUIRED)

add_executable(myelin_install_test main.cpp)
target_link_libraries(myelin_install_test PRIVATE MyeLiN::runtime)
target_compile_features(myelin_install_test PRIVATE cxx_std_23)
