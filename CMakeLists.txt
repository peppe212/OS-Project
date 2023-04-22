cmake_minimum_required(VERSION 3.17)
project(projectSO C)

set(CMAKE_C_STANDARD 99)

add_executable(client client.c shared.h server.h client.h queue.c queue.h shared.c)
add_executable(server server.c)