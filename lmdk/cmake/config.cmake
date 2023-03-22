# Default values for IMR and HPSRAM addresses. Overwrite them in
# project's CMakeLists.txt if needed.

if(NOT DEFINED IMR_ADDR)
  set(IMR_ADDR "0xa1608000")
  message("*** Default IMR address is used: ${IMR_ADDR}")
endif()

if(NOT DEFINED HPSRAM_ADDR)
  set(HPSRAM_ADDR "0xa06a1000")
  message("*** Default HPSRAM address is used: ${HPSRAM_ADDR}")
endif()
