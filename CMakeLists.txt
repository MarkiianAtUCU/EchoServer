cmake_minimum_required(VERSION 3.13)
project(EchoServer)

set(CMAKE_CXX_STANDARD 14)

set (Boost_USE_STATIC_LIBS OFF)
set (Boost_USE_MULTITHREAD ON)

find_package(Boost REQUIRED COMPONENTS system thread)

INCLUDE_DIRECTORIES(${Boost_INCLUDE_DIRS})
LINK_DIRECTORIES(${Boost_LIBRARY_DIRS})


add_executable(EchoServer main.cpp test.cpp)
target_link_libraries(EchoServer ${CMAKE_BOOST_LIBS_INIT} ${Boost_LIBRARIES})

add_executable(EchoServerOld working_version.cpp)
target_link_libraries(EchoServerOld ${CMAKE_BOOST_LIBS_INIT} ${Boost_LIBRARIES})

if(WIN32)
    target_link_libraries(EchoServer wsock32 ws2_32)
    target_link_libraries(EchoServerOld wsock32 ws2_32)
endif()