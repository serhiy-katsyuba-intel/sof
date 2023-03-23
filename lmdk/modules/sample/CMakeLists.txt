
target_sources(sample PRIVATE module_main.c foo.c moo.c)

set_target_properties(sample PROPERTIES
  IMR_ADDR "0xa1618000"
  HPSRAM_ADDR "0xa06b1000"
)
