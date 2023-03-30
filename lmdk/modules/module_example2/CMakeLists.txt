
target_sources(module_example2 PRIVATE module_main.c)

set_target_properties(module_example2 PROPERTIES
  HPSRAM_ADDR "0xa06a1000"
)
