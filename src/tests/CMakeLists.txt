# generated by cmake-generator
cmake_minimum_required(VERSION 3.16.3)

add_executable(pontoon_test
        main.c)

include(Tornado.cmake)
set_tornado(pontoon)

target_link_libraries(pontoon_test PUBLIC pontoon)
add_test(NAME PontoonTests COMMAND pontoon_test)
