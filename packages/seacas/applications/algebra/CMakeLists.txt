INCLUDE(FortranSettings)

TRIBITS_SUBPACKAGE(Algebra)

INCLUDE_DIRECTORIES(
  "${CMAKE_CURRENT_SOURCE_DIR}"
)

SET(HEADERS "")
APPEND_GLOB(SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/*.f)
TRIBITS_ADD_EXECUTABLE(
  algebra
  NOEXEPREFIX
  NOEXESUFFIX
  LINKER_LANGUAGE Fortran
  SOURCES ${SOURCES}
  COMM serial mpi
  INSTALLABLE
  )

TRIBITS_SUBPACKAGE_POSTPROCESS()
