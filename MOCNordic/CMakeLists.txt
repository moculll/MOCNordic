enable_language(CXX)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})

add_library(MOCNordic STATIC)
target_link_libraries(MOCNordic PRIVATE kernel zephyr_interface stdc++)
target_compile_features(MOCNordic PUBLIC cxx_std_17)
target_sources(MOCNordic PRIVATE
    MOCNordicBLE/MOCNordicBLEMgr.cpp
    MOCNordicHID/MOCNordicHIDevice.cpp
)

target_include_directories(MOCNordic PUBLIC 
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_include_directories(MOCNordic PRIVATE 
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)
