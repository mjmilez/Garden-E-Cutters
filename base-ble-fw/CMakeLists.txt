cmake_minimum_required(VERSION 3.5)

# Pull in shared components (log_transfer) from the repo root
set(EXTRA_COMPONENT_DIRS "../components")

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(base-ble-fw)