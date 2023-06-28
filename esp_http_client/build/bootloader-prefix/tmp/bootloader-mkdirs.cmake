# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file Copyright.txt or https://cmake.org/licensing for details.

cmake_minimum_required(VERSION 3.5)

file(MAKE_DIRECTORY
  "/home/rene/esp/esp-idf/components/bootloader/subproject"
  "/home/rene/Documents/ESP32_Projects/esp_http_client/build/bootloader"
  "/home/rene/Documents/ESP32_Projects/esp_http_client/build/bootloader-prefix"
  "/home/rene/Documents/ESP32_Projects/esp_http_client/build/bootloader-prefix/tmp"
  "/home/rene/Documents/ESP32_Projects/esp_http_client/build/bootloader-prefix/src/bootloader-stamp"
  "/home/rene/Documents/ESP32_Projects/esp_http_client/build/bootloader-prefix/src"
  "/home/rene/Documents/ESP32_Projects/esp_http_client/build/bootloader-prefix/src/bootloader-stamp"
)

set(configSubDirs )
foreach(subDir IN LISTS configSubDirs)
    file(MAKE_DIRECTORY "/home/rene/Documents/ESP32_Projects/esp_http_client/build/bootloader-prefix/src/bootloader-stamp/${subDir}")
endforeach()
if(cfgdir)
  file(MAKE_DIRECTORY "/home/rene/Documents/ESP32_Projects/esp_http_client/build/bootloader-prefix/src/bootloader-stamp${cfgdir}") # cfgdir has leading slash
endif()
