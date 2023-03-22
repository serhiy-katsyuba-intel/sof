# Linker scripts generator

# TODO: check if required parameters are atually specified:
# MODULES_LIST, LIBNAME, IMR_ADDR, HPSRAM_ADDR.

# reserve space for manifest?
math(EXPR IMR "${IMR_ADDR} + 9 * 4096" OUTPUT_FORMAT HEXADECIMAL)
math(EXPR HPSRAM "${HPSRAM_ADDR} + 9 * 4096" OUTPUT_FORMAT HEXADECIMAL)

separate_arguments(MODULES_LIST NATIVE_COMMAND ${MODULES_LIST})

file(MAKE_DIRECTORY ldscripts)
set(LDSCRIPT_FILE ldscripts/elf32xtensa.x)

file(WRITE ${LDSCRIPT_FILE} "")

file(APPEND ${LDSCRIPT_FILE} "INCLUDE ${CMAKE_CURRENT_BINARY_DIR}/ldscripts/memory_header_linker_script.txt\n")
configure_file(
  ${CMAKE_CURRENT_LIST_DIR}/ldscripts/memory_header_linker_script.txt.in
  ${CMAKE_CURRENT_BINARY_DIR}/ldscripts/memory_header_linker_script.txt
)

file(APPEND ${LDSCRIPT_FILE} "INCLUDE ${CMAKE_CURRENT_BINARY_DIR}/ldscripts/common_text_linker_script.txt\n")
file(COPY ${CMAKE_CURRENT_LIST_DIR}/ldscripts/common_text_linker_script.txt DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/ldscripts/)

file(APPEND ${LDSCRIPT_FILE} "INCLUDE ${CMAKE_CURRENT_BINARY_DIR}/ldscripts/common_rodata_linker_script.txt\n")
file(COPY ${CMAKE_CURRENT_LIST_DIR}/ldscripts/common_rodata_linker_script.txt DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/ldscripts/)

foreach(MODULE ${MODULES_LIST})
  # search for module's PackageEntryPoint function
  file(READ ${MODULE}.nm NM_CONTENTS)
  string(REGEX MATCH "[A-Za-z0-9_]+PackageEntryPoint" PACKAGE_ENTRY_POINT ${NM_CONTENTS})
  if(NOT PACKAGE_ENTRY_POINT)
    message(FATAL_ERROR "No xxxPackageEntryPoint function found in module ${MODULE}")
  endif()

  file(APPEND ${LDSCRIPT_FILE} "INCLUDE ${CMAKE_CURRENT_BINARY_DIR}/ldscripts/${MODULE}_text_linker_script.txt\n")
  configure_file(
    ${CMAKE_CURRENT_LIST_DIR}/ldscripts/module_text_linker_script.txt.in
    ${CMAKE_CURRENT_BINARY_DIR}/ldscripts/${MODULE}_text_linker_script.txt
  )

  file(APPEND ${LDSCRIPT_FILE} "INCLUDE ${CMAKE_CURRENT_BINARY_DIR}/ldscripts/${MODULE}_rodata_linker_script.txt\n")
  configure_file(
    ${CMAKE_CURRENT_LIST_DIR}/ldscripts/module_rodata_linker_script.txt.in
    ${CMAKE_CURRENT_BINARY_DIR}/ldscripts/${MODULE}_rodata_linker_script.txt
  )

  file(APPEND ${LDSCRIPT_FILE} "INCLUDE ${CMAKE_CURRENT_BINARY_DIR}/ldscripts/${MODULE}_bss_linker_script.txt\n")
  configure_file(
    ${CMAKE_CURRENT_LIST_DIR}/ldscripts/module_bss_linker_script.txt.in
    ${CMAKE_CURRENT_BINARY_DIR}/ldscripts/${MODULE}_bss_linker_script.txt
  )
endforeach()

file(APPEND ${LDSCRIPT_FILE} "INCLUDE ${CMAKE_CURRENT_BINARY_DIR}/ldscripts/xt_linker_script.txt\n")
file(COPY ${CMAKE_CURRENT_LIST_DIR}/ldscripts/xt_linker_script.txt  DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/ldscripts/)

file(APPEND ${LDSCRIPT_FILE} "INCLUDE ${CMAKE_CURRENT_BINARY_DIR}/ldscripts/guard_linker_script.txt\n")
configure_file(
  ${CMAKE_CURRENT_LIST_DIR}/ldscripts/guard_linker_script.txt.in
  ${CMAKE_CURRENT_BINARY_DIR}/ldscripts/guard_linker_script.txt
)
