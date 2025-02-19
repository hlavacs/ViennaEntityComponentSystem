
set(TARGET testvecs)

set(SOURCE testvecs.cpp testvecs_old.cpp)

set(HEADERS
  ${INCLUDE}/VECS.h
)

add_executable(${TARGET} ${SOURCE} ${HEADERS})

include_directories (${INCLUDE})
