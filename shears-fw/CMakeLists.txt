cmake_minimum_required(VERSION 3.16.0)

# Point to the shared components folder (one level up)
set(EXTRA_COMPONENT_DIRS "${CMAKE_CURRENT_LIST_DIR}/../components")

include($ENV{IDF_PATH}/tools/cmake/project.cmake)

project(shears_fw)
