# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file Copyright.txt or https://cmake.org/licensing for details.

cmake_minimum_required(VERSION 3.5)

file(MAKE_DIRECTORY
  "C:/Users/Hamza/esp/esp-idf/components/bootloader/subproject"
  "C:/Users/Hamza/Documents/esp-idf/Projects/WiFi/station-v5.1/build/bootloader"
  "C:/Users/Hamza/Documents/esp-idf/Projects/WiFi/station-v5.1/build/bootloader-prefix"
  "C:/Users/Hamza/Documents/esp-idf/Projects/WiFi/station-v5.1/build/bootloader-prefix/tmp"
  "C:/Users/Hamza/Documents/esp-idf/Projects/WiFi/station-v5.1/build/bootloader-prefix/src/bootloader-stamp"
  "C:/Users/Hamza/Documents/esp-idf/Projects/WiFi/station-v5.1/build/bootloader-prefix/src"
  "C:/Users/Hamza/Documents/esp-idf/Projects/WiFi/station-v5.1/build/bootloader-prefix/src/bootloader-stamp"
)

set(configSubDirs )
foreach(subDir IN LISTS configSubDirs)
    file(MAKE_DIRECTORY "C:/Users/Hamza/Documents/esp-idf/Projects/WiFi/station-v5.1/build/bootloader-prefix/src/bootloader-stamp/${subDir}")
endforeach()
if(cfgdir)
  file(MAKE_DIRECTORY "C:/Users/Hamza/Documents/esp-idf/Projects/WiFi/station-v5.1/build/bootloader-prefix/src/bootloader-stamp${cfgdir}") # cfgdir has leading slash
endif()