
set(TARGET testvecs)

set(SOURCE testvecs.cpp)

set(HEADERS
  ${INCLUDE}/VECS.h
)

add_executable(${TARGET} ${SOURCE} ${HEADERS})

include_directories (${INCLUDE})
