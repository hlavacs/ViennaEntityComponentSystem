
set(TARGET test)

set(SOURCE test.cpp)

set(HEADERS
  test.h
  ${INCLUDE}/VECS.h
  ${EXTERNAL}/ViennaTypeListLibrary/VTLL.h
)

add_executable(${TARGET} ${SOURCE} ${HEADERS})

target_include_directories (${TARGET} PRIVATE ${EXTERNAL}/ViennaTypeListLibrary ${INCLUDE})

target_compile_features(${TARGET} PUBLIC cxx_std_20)
