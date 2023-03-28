# This file is intended to be included from project's CMakeLists.txt.
# Prior to include, MODULES_LIST variable should be initialised with list
# of modules (subdirectories in modules dir) that should be linked into
# project's loadable library.

include(${CMAKE_CURRENT_LIST_DIR}/config.cmake)

foreach(MODULE ${MODULES_LIST})
  add_executable(${MODULE})
  add_subdirectory(${LMDK_BASE}/modules/${MODULE} ${MODULE}_module)

###  set_target_properties(${MODULE} PROPERTIES OUTPUT_NAME ${MODULE}.mod)

  target_include_directories(${MODULE} PRIVATE
    "${LMDK_BASE}/include"
    "${RIMAGE_INCLUDE_DIR}"
  )

  # generate linker script
  get_target_property(IMR_ADDR ${MODULE} IMR_ADDR)
  get_target_property(HPSRAM_ADDR ${MODULE} HPSRAM_ADDR)

  if(NOT DEFINED IMR_ADDR OR NOT DEFINED HPSRAM_ADDR)
    message(FATAL_ERROR "Please define IMR_ADDR and HPSRAM_ADDR for module ${MODULE}: '${IMR_ADDR}' '${HPSRAM_ADDR}'")
  endif()

  add_custom_command(TARGET ${MODULE} PRE_LINK
    COMMAND ${CMAKE_COMMAND}
      -DMODULE=${MODULE}
      -DIMR_ADDR=${IMR_ADDR}
      -DHPSRAM_ADDR=${HPSRAM_ADDR}
      -P ${CMAKE_CURRENT_LIST_DIR}/ldscripts.cmake
  )

  target_link_options(${MODULE} PRIVATE
    "--verbose"	# optional
    "-nostdlib" "-nodefaultlibs"
    "-Wl,--no-undefined" "-Wl,--unresolved-symbols=report-all" "-Wl,--error-unresolved-symbols"
    #"-Wl,--gc-sections"	# may remove .bss and that will result in rimage error, do not used for now
    "-Wl,-Map,$<TARGET_FILE:${MODULE}>.map"	# optional: just for debug
    "-T" "${MODULE}_ldscripts/elf32xtensa.x"
  )
endforeach()

set(LIBRARY ${PROJECT_NAME}_target)
set(OUTPUT_FILE ${PROJECT_NAME}.bin)

add_custom_target(${LIBRARY} ALL
  DEPENDS ${MODULES_LIST}
  COMMAND ${RIMAGE_COMMAND} -v -k ${SIGNING_KEY} -f 2.0.0 -b 1 -o ${OUTPUT_FILE} -c ${TOML} -e ${MODULES_LIST}
)
