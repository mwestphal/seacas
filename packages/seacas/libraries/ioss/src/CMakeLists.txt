IF (${PROJECT_NAME}_ENABLE_Kokkos)
  SET(SEACAS_HAVE_KOKKOS ON)
ENDIF()

IF (${PROJECT_NAME}_ENABLE_Pamgen)
  SET(SEACAS_HAVE_PAMGEN ON)
ENDIF()

IF (TPL_ENABLE_MPI)
  SET(SEACAS_HAVE_MPI ON)
ENDIF()

IF (TPL_ENABLE_DataWarp)
  SET(SEACAS_HAVE_DATAWARP ON)
ENDIF()

IF (TPL_ENABLE_CGNS)
  SET(SEACAS_HAVE_CGNS ON)
ENDIF()

IF (${TPL_ENABLE_Faodel})
  SET(SEACAS_HAVE_FAODEL ON)
ENDIF()

IF (TPL_ENABLE_Catalyst2)
  SET(SEACAS_HAVE_CATALYST2 ON)
ENDIF()

IF (${PACKAGE_NAME}_ENABLE_SEACASExodus)
  SET(SEACAS_HAVE_EXODUS ON)
ENDIF()

TRIBITS_CONFIGURE_FILE(${PACKAGE_NAME}_config.h)

SET(HEADERS "")
SET(SOURCES "")

SET(HEADERS ${HEADERS}
  ${CMAKE_CURRENT_BINARY_DIR}/${PACKAGE_NAME}_config.h)


ASSERT_DEFINED(${PROJECT_NAME}_ENABLE_Zoltan)
IF (NOT ${PROJECT_NAME}_ENABLE_Zoltan)
  ADD_DEFINITIONS(-DNO_ZOLTAN_SUPPORT)
ENDIF()

IF (NOT TPL_ENABLE_ParMETIS)
  ADD_DEFINITIONS(-DNO_PARMETIS_SUPPORT)
ENDIF()

SET_AND_INC_DIRS(DIR ${CMAKE_CURRENT_SOURCE_DIR})
APPEND_GLOB(HEADERS ${DIR}/Ioss_*.h)
APPEND_GLOB(HEADERS ${DIR}/*hopscotch*.h)
APPEND_GLOB(HEADERS ${DIR}/robin*.h)
APPEND_GLOB(HEADERS ${DIR}/pdq*.h)
APPEND_GLOB(SOURCES ${DIR}/*.C)
APPEND_GLOB(SOURCES ${DIR}/*.c)

IF (NOT TPL_ENABLE_MPI)
  LIST(REMOVE_ITEM SOURCES ${DIR}/Ioss_Decomposition.C)
  LIST(REMOVE_ITEM HEADERS ${DIR}/Ioss_Decomposition.h)
ENDIF()

INCLUDE_DIRECTORIES(
  "${CMAKE_CURRENT_SOURCE_DIR}"
  "${CMAKE_CURRENT_BINARY_DIR}"
  "${Zoltan_INCLUDE_DIRS}"
  )

TRIBITS_ADD_LIBRARY(
  Ioss
  HEADERS ${HEADERS}
  SOURCES ${SOURCES}
  )
IF (MINGW)
  target_link_libraries(Ioss psapi)
ENDIF ()

if (${CMAKE_PROJECT_NAME} STREQUAL "SEACASProj")
   # add a target to generate API documentation with Doxygen
   if(SEACASProj_ENABLE_DOXYGEN)
     find_package(Doxygen)
     if(DOXYGEN_FOUND)
       add_custom_command(TARGET Ioss
          COMMAND ${DOXYGEN_EXECUTABLE} ARGS Doxyfile
          WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/..
          COMMENT "Generating IOSS API documentation with Doxygen" VERBATIM
        )
     endif(DOXYGEN_FOUND)
   endif()
endif()

IF (${PACKAGE_NAME}_ENABLE_SEACASExodus)
  ADD_SUBDIRECTORY(exodus)
ENDIF()

IF (${PROJECT_NAME}_ENABLE_Pamgen)
  ADD_SUBDIRECTORY(pamgen)
ENDIF()

IF (TPL_ENABLE_CGNS)
  ADD_SUBDIRECTORY(cgns)
ENDIF()

IF (${TPL_ENABLE_Faodel})
  ADD_SUBDIRECTORY(faodel)
ENDIF()

IF (TPL_ENABLE_ADIOS2)
  ADD_SUBDIRECTORY(adios)
ENDIF()

IF (TPL_ENABLE_Catalyst2)
  ADD_SUBDIRECTORY(catalyst)
ENDIF()

ADD_SUBDIRECTORY(visualization)
ADD_SUBDIRECTORY(generated)
ADD_SUBDIRECTORY(text_mesh)
ADD_SUBDIRECTORY(gen_struc)
ADD_SUBDIRECTORY(heartbeat)
ADD_SUBDIRECTORY(transform)
ADD_SUBDIRECTORY(init)
ADD_SUBDIRECTORY(main)

TRIBITS_ADD_TEST_DIRECTORIES(utest)

IF (TPL_ENABLE_GTest)
  TRIBITS_ADD_TEST_DIRECTORIES(unit_tests)
ENDIF()
