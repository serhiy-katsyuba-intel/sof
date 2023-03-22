# This file is intended to be included from project's CMakeLists.txt.
# Prior to include, MODULES_LIST variable should be initialised with list
# of modules (subdirectories in modules dir) that should be linked into
# project's loadable library.

include(${CMAKE_CURRENT_LIST_DIR}/config.cmake)

foreach(MODULE ${MODULES_LIST})
  add_library(${MODULE} STATIC)
  add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/../modules/${MODULE} ${MODULE}_module)

  target_include_directories(${MODULE} PRIVATE
    "${CMAKE_CURRENT_LIST_DIR}/../include"
    "${CMAKE_CURRENT_LIST_DIR}/../../src/include/sof/audio/module_adapter/iadk"	# REMOVE THIS???
  )

  add_custom_command(TARGET ${MODULE} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:${MODULE}> $<TARGET_FILE:${MODULE}>.original.a
    COMMAND ${CMAKE_OBJCOPY} --prefix-alloc-sections=.${MODULE} $<TARGET_FILE:${MODULE}>.original.a $<TARGET_FILE:${MODULE}>
    COMMAND ${CMAKE_NM} $<TARGET_FILE:${MODULE}> > ${MODULE}.nm
  )
endforeach()

# TODO: modify to eliminate _library in target exe file name (allow user to decide how its binary should be named)!!!
set(LIBRARY ${PROJECT_NAME}_library)
add_executable(${LIBRARY} ${CMAKE_CURRENT_LIST_DIR}/empty.c)

# generate linker scripts
add_custom_command(TARGET ${LIBRARY} PRE_LINK DEPENDS ${MODULES_LIST}
  COMMAND ${CMAKE_COMMAND}
    -DMODULES_LIST="${MODULES_LIST}"
    -DLIBNAME=${LIBRARY}
    -DIMR_ADDR=${IMR_ADDR}
    -DHPSRAM_ADDR=${HPSRAM_ADDR}
    -P ${CMAKE_CURRENT_LIST_DIR}/ldscripts.cmake
)

target_link_options(${LIBRARY} PRIVATE "--verbose")	# optional
target_link_options(${LIBRARY} PRIVATE "-nostdlib" "-nodefaultlibs")
target_link_options(${LIBRARY} PRIVATE "-Wl,--no-undefined" "-Wl,--unresolved-symbols=report-all" "-Wl,--error-unresolved-symbols")
##target_link_options(${LIBRARY} PRIVATE "-Wl,--gc-sections")
target_link_options(${LIBRARY} PRIVATE "-Wl,-Map,$<TARGET_FILE:${LIBRARY}>.map")
target_link_options(${LIBRARY} PRIVATE "-T" "ldscripts/elf32xtensa.x")

target_link_libraries(${LIBRARY} PRIVATE ${MODULES_LIST})
