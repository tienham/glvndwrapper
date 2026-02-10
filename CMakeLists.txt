cmake_minimum_required(VERSION 2.8)

project(eglwrapper)

add_definitions(-D_FILE_OFFSET_BITS=64)


# find_package (Python3 COMPONENTS Development)



add_library(EGL_mali SHARED egl_wrapper.c)

target_include_directories(EGL_mali
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(EGL_mali dl)


install(TARGETS EGL_mali)

install(FILES ${INSTALL_HEAD_FILES} DESTINATION  /usr/include)
install(FILES ${PYTHON_BIN} DESTINATION  /usr/bin)
