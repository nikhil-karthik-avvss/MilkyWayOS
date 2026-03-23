cmake_minimum_required(VERSION 3.16)
project(starlight C)

find_package(PkgConfig REQUIRED)
pkg_check_modules(DRM REQUIRED libdrm)

add_executable(starlight starlight.c)
target_include_directories(starlight PRIVATE ${DRM_INCLUDE_DIRS})
target_link_libraries(starlight ${DRM_LIBRARIES})

install(TARGETS starlight DESTINATION /usr/local/bin)
