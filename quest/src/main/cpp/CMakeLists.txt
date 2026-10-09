cmake_minimum_required(VERSION 3.22)
project(telepepper LANGUAGES C CXX)
find_package(OpenXR REQUIRED CONFIG)
add_library(native_app_glue STATIC ${ANDROID_NDK}/sources/android/native_app_glue/android_native_app_glue.c)
target_include_directories(native_app_glue PUBLIC ${ANDROID_NDK}/sources/android/native_app_glue)
add_library(telepepper SHARED main.cpp network.cpp audio.cpp)
# Quoted CMake options preserve paths containing spaces and remove personal
# source paths from vendor __FILE__ assertions in the distributed binary.
get_filename_component(TELEPEPPER_SOURCE_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../../../.." ABSOLUTE)
target_compile_options(telepepper PRIVATE "-ffile-prefix-map=${TELEPEPPER_SOURCE_ROOT}=.")
target_include_directories(telepepper PRIVATE ../../../../third_party)
target_compile_definitions(telepepper PRIVATE XR_USE_PLATFORM_ANDROID XR_USE_GRAPHICS_API_OPENGL_ES)
target_link_libraries(telepepper native_app_glue OpenXR::openxr_loader android log EGL GLESv3 aaudio)
target_link_options(telepepper PRIVATE "-uANativeActivity_onCreate")
