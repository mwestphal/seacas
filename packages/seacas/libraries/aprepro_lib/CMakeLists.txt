
option(USE_TRIBITS "Use the Tribits modules to build Aprepro" ON)
if(USE_TRIBITS)

  TRIBITS_SUBPACKAGE(Aprepro_lib)
endif()

SET(APREPRO_VERSION_MAJOR "4")
SET(APREPRO_VERSION_MINOR "02")
SET(APREPRO_VERSION_PATCH "0")
SET(APREPRO_VERSION "${APREPRO_VERSION_MAJOR}.${APREPRO_VERSION_MINOR}")
SET(APREPRO_VERSION_FULL "${APREPRO_VERSION}.${APREPRO_VERSION_PATCH}")

IF (${CMAKE_PROJECT_NAME}_ENABLE_SEACASExodus)
  ADD_DEFINITIONS(-DEXODUS_SUPPORT)
ENDIF()

INCLUDE_DIRECTORIES(
  ${CMAKE_CURRENT_SOURCE_DIR}
)

option(GENERATE_FLEX_FILES "Generate source files using flex" OFF)
option(GENERATE_BISON_FILES "Generate source files using bison" OFF)

if(GENERATE_BISON_FILES)
  set(BISON_LIB_SOURCE ${CMAKE_CURRENT_BINARY_DIR}/apr_parser.cc)
else()
  set(BISON_LIB_SOURCE ${CMAKE_CURRENT_SOURCE_DIR}/apr_parser.cc)
endif()

SET(HEADERS
  aprepro.h
)

SET(SOURCES
 apr_aprepro.cc
 apr_util.cc
 apr_init.cc
 apr_builtin.cc
 apr_units.cc
 apr_tokenize.cc
 apr_scanner.cc
 apr_getline_int.c
 ${BISON_LIB_SOURCE}
 apr_exodus.cc
 apr_array.cc
)

if(GENERATE_FLEX_FILES)
  find_program(FLEX_EXE NAMES flex
               DOC "Flex Executable")
  if(NOT FLEX_EXE)
    message(SEND_ERROR "The CMake variable FLEX_EXE does not point to a valid flex executable.")
  endif()

  add_custom_command(OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/apr_scanner.cc
                     COMMAND ${FLEX_EXE}
                     ARGS -o apr_scanner.cc --noline
		        ${CMAKE_CURRENT_SOURCE_DIR}/aprepro.ll
                     MAIN_DEPENDENCY aprepro.ll
                     WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
                     )
endif()

if(GENERATE_BISON_FILES)
  find_program(BISON_EXE NAMES bison
               DOC "Bison Executable")
  if(NOT BISON_EXE)
    message(SEND_ERROR "The CMake variable BISON_EXE does not point to a valid bison executable.")
  endif()

  if(WIN32)
    get_filename_component(BISON_PATH ${BISON_EXE} PATH)
    set(ENV{PATH} "${BISON_PATH};$ENV{PATH}")
  endif(WIN32)

  add_custom_command(OUTPUT  apr_parser.cc
                     COMMAND ${BISON_EXE}
                     ARGS --defines=aprepro_parser.h --output apr_parser.cc
                        ${CMAKE_CURRENT_SOURCE_DIR}/aprepro.yy
                     MAIN_DEPENDENCY aprepro.yy
                     WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
                     )
endif()

if(USE_TRIBITS)
  TRIBITS_ADD_LIBRARY(
    aprepro_lib
    HEADERS	${HEADERS}
    SOURCES ${SOURCES}
  )

if (${CMAKE_PROJECT_NAME} STREQUAL "SEACASProj")
   # add a target to generate API documentation with Doxygen
   if(SEACASProj_ENABLE_DOXYGEN)
     find_package(Doxygen)
     if(DOXYGEN_FOUND)
       add_custom_command(TARGET aprepro_lib POST_BUILD
          COMMAND ${DOXYGEN_EXECUTABLE} ARGS ${CMAKE_CURRENT_SOURCE_DIR}/Doxyfile
          WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
          COMMENT "Generating Aprepro API documentation with Doxygen" VERBATIM
        )
     endif(DOXYGEN_FOUND)
   endif()
endif()

if (${CMAKE_PROJECT_NAME}_ENABLE_TESTS)
  TRIBITS_ADD_EXECUTABLE(aprepro_test_app NOEXEPREFIX NOEXESUFFIX SOURCES apr_test.cc LINKER_LANGUAGE CXX)

TRIBITS_ADD_ADVANCED_TEST(
 aprepro_lib_unit_test
 TEST_0 EXEC aprepro_test_app
        ARGS -o test.output ${CMAKE_CURRENT_SOURCE_DIR}/test.inp_app
  NOEXEPREFIX NOEXESUFFIX
  PASS_ANY
 TEST_1 CMND diff ARGS -w
		       ${CMAKE_CURRENT_SOURCE_DIR}/test_standard.out
		       ${CMAKE_CURRENT_BINARY_DIR}/test.output
 COMM mpi serial
 OVERALL_NUM_MPI_PROCS 1
 FINAL_PASS_REGULAR_EXPRESSION
 XHOSTTYPE Windows
)

TRIBITS_ADD_ADVANCED_TEST(
 aprepro_lib_array_test
 TEST_0 EXEC aprepro_test_app
	ARGS --info=test-array.dump --include=${CMAKE_CURRENT_SOURCE_DIR} -o test-array.out
	     ${CMAKE_CURRENT_SOURCE_DIR}/test-array.i
  NOEXEPREFIX NOEXESUFFIX
  PASS_ANY
 TEST_1 CMND diff ARGS -w
		       ${CMAKE_CURRENT_SOURCE_DIR}/test-array.gold
		       ${CMAKE_CURRENT_BINARY_DIR}/test-array.out
 TEST_2 CMND diff ARGS -w
		       ${CMAKE_CURRENT_SOURCE_DIR}/test-array.stderr.gold
		       ${CMAKE_CURRENT_BINARY_DIR}/test-array.dump
 COMM mpi serial
 OVERALL_NUM_MPI_PROCS 1
 FINAL_PASS_REGULAR_EXPRESSION
 XHOSTTYPE Windows
)
endif()

  TRIBITS_SUBPACKAGE_POSTPROCESS()

else()
  add_library(aprepro_lib ${HEADERS} ${SOURCES})
endif()
