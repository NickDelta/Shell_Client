cmake_minimum_required(VERSION 3.15)
project(Client C)

set(CMAKE_C_STANDARD 99)

include_directories(.)

add_executable(Client
        Client.c
        Client.h)
