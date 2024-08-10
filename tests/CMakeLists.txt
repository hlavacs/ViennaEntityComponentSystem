
set(TARGET testlib)

set(SOURCE testlib.cpp)

set(HEADERS
  ${INCLUDE}/VECS.h
)

add_executable(${TARGET} ${SOURCE} ${HEADERS})

include_directories (${INCLUDE})
