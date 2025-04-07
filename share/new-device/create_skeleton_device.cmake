# call cmake in script mode with -P option

# cmake \
# -DCMAKE_INSTALL_PREFIX=<path-to-output> \
# -DDevice=<device-name>  \
#  -P new-device/create_skeleton_device.cmake

foreach(ldev IN ITEMS ${Device})
  message("ldev = ${ldev}")
  foreach(lsuffix IN ITEMS h;cxx)
    # file(READ <filename> <out-var> [...])
    file(READ "${CMAKE_CURRENT_LIST_DIR}/skeleton/Skeleton.${lsuffix}" InputContent)

    # string(REPLACE <match-string> <replace-string> <out-var> <input> ...)
    string(REPLACE "Skeleton" "${ldev}" OutputContent "${InputContent}")

    # file(WRITE <filename> <content> ...)
    file(WRITE "${CMAKE_INSTALL_PREFIX}/${ldev}.${lsuffix}" "${OutputContent}")
  endforeach() # foreach(lsuffix IN ITEMS h;cxx)
endforeach() # foreach(ldev IN ITEMS "${DEVICE}")

file(READ "${CMAKE_CURRENT_LIST_DIR}/skeleton/CMakeLists.txt" InputContent)
string(REPLACE "DeviceListPlaceHolder" "${Device}" OutputContent "${InputContent}")
file(WRITE "${CMAKE_INSTALL_PREFIX}/CMakeLists.txt" "${OutputContent}")

file(REMOVE_RECURSE "${CMAKE_INSTALL_PREFIX}/cmake")
file(COPY "${CMAKE_CURRENT_LIST_DIR}/skeleton/cmake" DESTINATION "${CMAKE_INSTALL_PREFIX}")