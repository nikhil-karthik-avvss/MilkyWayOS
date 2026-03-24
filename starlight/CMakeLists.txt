cmake_minimum_required(VERSION 3.16)
project(starlight C)

find_package(PkgConfig REQUIRED)
pkg_check_modules(DRM REQUIRED libdrm)
pkg_check_modules(LIBINPUT REQUIRED libinput)
pkg_check_modules(UDEV REQUIRED libudev)

add_executable(starlight
    main.c
    display.c
    draw.c
    input.c
    window.c
)

target_include_directories(starlight PRIVATE
    ${DRM_INCLUDE_DIRS}
    ${LIBINPUT_INCLUDE_DIRS}
    ${UDEV_INCLUDE_DIRS}
)

target_link_libraries(starlight
    ${DRM_LIBRARIES}
    ${LIBINPUT_LIBRARIES}
    ${UDEV_LIBRARIES}
)

install(TARGETS starlight DESTINATION /usr/local/bin)
