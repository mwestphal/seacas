if(CMAKE_PROJECT_NAME STREQUAL "SEACAS")
  SET(LIB_OPTIONAL_DEP_PACKAGES)
  SET(LIB_OPTIONAL_DEP_TPLS)
else()
  SET(LIB_OPTIONAL_DEP_PACKAGES ${SEACAS_PREFIX}Exodus Pamgen)
  SET(LIB_OPTIONAL_DEP_TPLS XDMF HDF5)
endif()

SET(LIB_REQUIRED_DEP_PACKAGES)
SET(TEST_REQUIRED_DEP_PACKAGES)
SET(TEST_OPTIONAL_DEP_PACKAGES)
SET(LIB_REQUIRED_DEP_TPLS)
SET(TEST_REQUIRED_DEP_TPLS)
SET(TEST_OPTIONAL_DEP_TPLS)
