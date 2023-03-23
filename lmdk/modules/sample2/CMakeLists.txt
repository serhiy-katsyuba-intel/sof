
target_sources(sample2 PRIVATE module_main.c boo.c foo.c)

set_target_properties(sample2 PROPERTIES
  IMR_ADDR "0xa1628000"
  HPSRAM_ADDR "0xa06c1000"
)
