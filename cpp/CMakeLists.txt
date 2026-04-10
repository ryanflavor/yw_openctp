cmake_minimum_required(VERSION 3.16)
project(openctp_cpp_tests LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CTP_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/deps/include)
set(CTP_LIB_DIR     ${CMAKE_CURRENT_SOURCE_DIR}/deps/lib)

add_executable(test_td test_td.cpp)
target_include_directories(test_td PRIVATE ${CTP_INCLUDE_DIR})
target_link_directories(test_td PRIVATE ${CTP_LIB_DIR})
target_link_libraries(test_td thosttraderapi_se pthread)

add_executable(test_md test_md.cpp)
target_include_directories(test_md PRIVATE ${CTP_INCLUDE_DIR})
target_link_directories(test_md PRIVATE ${CTP_LIB_DIR})
target_link_libraries(test_md thostmduserapi_se pthread)

install(TARGETS test_td test_md DESTINATION bin)
