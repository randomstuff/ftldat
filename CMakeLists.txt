cmake_minimum_required (VERSION 2.8.11)
project(ftldat C)
set(CMAKE_C_FLAGS -std=gnu99 ${CMAKE_C_FLAGS})
add_executable(ftldat src/ftldat.c)
