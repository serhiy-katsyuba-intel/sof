# Linker scripts generator

# TODO: check if required parameters are atually specified: HPSRAM_ADDR and MODULE.

# reserve space for manifest?
math(EXPR HPSRAM "${HPSRAM_ADDR} + 9 * 4096" OUTPUT_FORMAT HEXADECIMAL)

set(LDSCRIPTS_DIR ${MODULE}_ldscripts)
set(LDSCRIPT_FILE ${LDSCRIPTS_DIR}/elf32xtensa.x)

file(MAKE_DIRECTORY ${LDSCRIPTS_DIR})
file(WRITE ${LDSCRIPT_FILE} "")

file(APPEND ${LDSCRIPT_FILE} "INCLUDE ${CMAKE_CURRENT_BINARY_DIR}/${LDSCRIPTS_DIR}/memory_header_linker_script.txt\n")
configure_file(
  ${CMAKE_CURRENT_LIST_DIR}/ldscripts/memory_header_linker_script.txt.in
  ${CMAKE_CURRENT_BINARY_DIR}/${LDSCRIPTS_DIR}/memory_header_linker_script.txt
)

file(APPEND ${LDSCRIPT_FILE} "INCLUDE ${CMAKE_CURRENT_LIST_DIR}/ldscripts/text_linker_script.txt\n")
file(APPEND ${LDSCRIPT_FILE} "INCLUDE ${CMAKE_CURRENT_LIST_DIR}/ldscripts/common_text_linker_script.txt\n")
file(APPEND ${LDSCRIPT_FILE} "INCLUDE ${CMAKE_CURRENT_LIST_DIR}/ldscripts/data_linker_script.txt\n")
file(APPEND ${LDSCRIPT_FILE} "INCLUDE ${CMAKE_CURRENT_LIST_DIR}/ldscripts/common_rodata_linker_script.txt\n")
file(APPEND ${LDSCRIPT_FILE} "INCLUDE ${CMAKE_CURRENT_LIST_DIR}/ldscripts/bss_linker_script.txt\n")
file(APPEND ${LDSCRIPT_FILE} "INCLUDE ${CMAKE_CURRENT_LIST_DIR}/ldscripts/xt_linker_script.txt\n")
file(APPEND ${LDSCRIPT_FILE} "INCLUDE ${CMAKE_CURRENT_LIST_DIR}/ldscripts/guard_linker_script.txt\n")
