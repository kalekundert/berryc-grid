cmake_minimum_required(VERSION 3.10)
project(berryc-grid)

add_executable(berryc-grid berryc-grid.c)
target_link_libraries(berryc-grid X11 cjson)
install(TARGETS berryc-grid)


