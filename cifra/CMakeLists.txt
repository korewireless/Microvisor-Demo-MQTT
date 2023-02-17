cmake_minimum_required(VERSION 3.12)

set(CMAKE_TOOLCHAIN_FILE "${CMAKE_CURRENT_SOURCE_DIR}/toolchain.cmake")

project(cifra VERSION 0.0.1 DESCRIPTION "embedded cryptography library")

set(MV_ARCH "stm32u5")

add_library(cifra STATIC
    cifra/src/blockwise.c
    cifra/src/chash.c
    cifra/src/hmac.c
    cifra/src/sha256.c
)

set_target_properties(cifra PROPERTIES VERSION ${PROJECT_VERSION})

set_target_properties(cifra PROPERTIES SOVERSION 1)

set_property(TARGET cifra PROPERTY COMPILE_DEFINITIONS FULL_FAT_ASSERT)

target_include_directories(cifra PUBLIC cifra/src)
target_include_directories(cifra PUBLIC cifra/src/ext)

install(TARGETS ${PROJECT_NAME} 
    LIBRARY DESTINATION lib/${PROJECT_NAME}
    ARCHIVE DESTINATION lib/${PROJECT_NAME}
    PUBLIC_HEADER DESTINATION include)
