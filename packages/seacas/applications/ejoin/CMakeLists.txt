
TRIBITS_SUBPACKAGE(Ejoin)

INCLUDE_DIRECTORIES(
  "${CMAKE_CURRENT_SOURCE_DIR}"
)

SET(HEADERS "")
APPEND_GLOB(SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/*.C)
TRIBITS_ADD_EXECUTABLE(
  ejoin
  NOEXEPREFIX
  NOEXESUFFIX
  SOURCES ${SOURCES}
  COMM serial mpi
  INSTALLABLE
  )

TRIBITS_SUBPACKAGE_POSTPROCESS()
