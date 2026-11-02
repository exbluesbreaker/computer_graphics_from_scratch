# stb_image_write: single-header PNG writer, public domain (see stb/README.md).
# It is compiled into its own library so the implementation is built without the
# project's warning flags, and its include directory is SYSTEM so including the header
# does not trip them either.
add_library(stb_image_write STATIC stb/stb_image_write.cpp)
target_include_directories(stb_image_write SYSTEM PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/stb)
