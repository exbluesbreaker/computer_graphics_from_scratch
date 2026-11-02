add_executable(render_demo render_demo.cpp)
target_link_libraries(render_demo PRIVATE cgfs::cgfs)
cgfs_set_warnings(render_demo)
