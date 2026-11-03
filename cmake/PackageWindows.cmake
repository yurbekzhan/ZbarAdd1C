if(NOT EXISTS "${COMPONENT}")
  message(FATAL_ERROR "Component not built: ${COMPONENT}")
endif()
file(MAKE_DIRECTORY "${DESTINATION}")
file(COPY "${COMPONENT}" DESTINATION "${DESTINATION}")
configure_file("${SOURCE}/resources/Manifest.xml" "${DESTINATION}/Manifest.xml" COPYONLY)
configure_file("${SOURCE}/third_party/sources/README.md" "${DESTINATION}/DEPENDENCIES.txt" COPYONLY)
file(MAKE_DIRECTORY "${DESTINATION}/licenses")
configure_file("${DEPENDENCIES}/zbar/zbar/COPYING" "${DESTINATION}/licenses/ZBar-LGPL.txt" COPYONLY)
configure_file("${DEPENDENCIES}/png/libpng/LICENSE" "${DESTINATION}/licenses/libpng.txt" COPYONLY)
configure_file("${DEPENDENCIES}/zlib/zlib/LICENSE" "${DESTINATION}/licenses/zlib.txt" COPYONLY)
# Include the full source project in the distribution, so the statically linked
# LGPL dependency can be modified and the component rebuilt/relinked.
file(MAKE_DIRECTORY "${DESTINATION}/source")
foreach(entry CMakeLists.txt CMakePresets.json README.md cmake resources src include third_party tests tools examples)
  file(COPY "${SOURCE}/${entry}" DESTINATION "${DESTINATION}/source")
endforeach()
execute_process(COMMAND "${CMAKE_COMMAND}" -E tar cf ZbarAdd1C-win64.zip --format=zip
    ZbarAdd1C.dll Manifest.xml DEPENDENCIES.txt licenses source
    WORKING_DIRECTORY "${DESTINATION}" RESULT_VARIABLE result)
if(NOT result EQUAL 0)
  message(FATAL_ERROR "Failed to create Windows bundle")
endif()
