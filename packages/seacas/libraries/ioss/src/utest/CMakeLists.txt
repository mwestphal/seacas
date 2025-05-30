
TRIBITS_ADD_EXECUTABLE(
 Utst_utils
 SOURCES Utst_utils.C
)

TRIBITS_ADD_TEST(
	Utst_utils
	NAME Utst_utils
	NUM_MPI_PROCS 1
)

TRIBITS_ADD_EXECUTABLE(
 Utst_iofx
 SOURCES Utst_IofxDatabaseIO.C
)

TRIBITS_ADD_TEST(
	Utst_iofx
	NAME Utst_iofx
	ARGS "--filename ${CMAKE_CURRENT_SOURCE_DIR}/ADeDA.e"
	NUM_MPI_PROCS 1
)

TRIBITS_ADD_EXECUTABLE(
 Utst_ioel
 SOURCES Utst_ioel.C
)

TRIBITS_ADD_TEST(
	Utst_ioel
	NAME Utst_ioel
	NUM_MPI_PROCS 1
)

TRIBITS_ADD_EXECUTABLE(
 Utst_blob_write_read
 SOURCES Utst_blob_write_read.C
)

TRIBITS_ADD_TEST(
        Utst_blob_write_read
	NAME Utst_blob_write_read
	NUM_MPI_PROCS ${NPROCS}
	COMM mpi serial
	XHOSTTYPE Windows
  )

IF (NOT SEACASIoss_ENABLE_THREADSAFE)
TRIBITS_ADD_EXECUTABLE(
 Utst_sort
 SOURCES Utst_sort.C
)

TRIBITS_ADD_TEST(
	Utst_sort
	NAME Utst_sort
	NUM_MPI_PROCS 1
)
ENDIF()

TRIBITS_ADD_EXECUTABLE(
 Utst_map
 SOURCES Utst_map.C
)

TRIBITS_ADD_TEST(
	Utst_map
	NAME Utst_map
	NUM_MPI_PROCS 1
)

IF (TPL_ENABLE_CGNS)
SET_SOURCE_FILES_PROPERTIES(Utst_structured_decomp.C PROPERTIES COMPILE_FLAGS -O0)
SET_SOURCE_FILES_PROPERTIES(Utst_structured_decomp_rocket.C PROPERTIES COMPILE_FLAGS -O0)
SET_SOURCE_FILES_PROPERTIES(Utst_structured_decomp_herron.C PROPERTIES COMPILE_FLAGS -O0)
TRIBITS_ADD_EXECUTABLE(
 Utst_structured_decomp
 SOURCES Utst_structured_decomp.C Utst_structured_decomp_herron.C Utst_structured_decomp_rocket.C
)

IF (NOT SEACASIoss_ENABLE_THREADSAFE)
TRIBITS_ADD_TEST(
	Utst_structured_decomp
	NAME Utst_structured_decomp
	NUM_MPI_PROCS 1
)
ENDIF()
ENDIF()

IF (${PACKAGE_NAME}_ENABLE_SEACASExodus)
TRIBITS_ADD_EXECUTABLE(
 Utst_superelement
 SOURCES Utst_superelement.C
)

TRIBITS_ADD_TEST(
	Utst_superelement
	NAME Utst_superelement
	NUM_MPI_PROCS 1
	ARGS ${CMAKE_CURRENT_SOURCE_DIR}/cbr2.ncf
)
ENDIF()

INCLUDE_DIRECTORIES(
  "${CMAKE_CURRENT_SOURCE_DIR}/../"
  "${CMAKE_CURRENT_BINARY_DIR}/../"
)
