
set(TARGET test)

set(SOURCE test.cpp)

set(HEADERS
  test.h
  ${INCLUDE}/VTLL.h
  ${INCLUDE}/IntType.h
  ${INCLUDE}/VECSUtil.h
  ${INCLUDE}/VECSTable.h
  ${INCLUDE}/VECS.h
  ${INCLUDE}/VECSIterator.h
)

add_executable(${TARGET} ${SOURCE} ${HEADERS})

target_compile_features(${TARGET} PUBLIC cxx_std_20)
