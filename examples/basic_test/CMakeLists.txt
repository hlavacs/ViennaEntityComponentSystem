
set(TARGET basic_test)

set(SOURCE basic_test.cpp)

set(HEADERS
  basic_test.h
  ${INCLUDE}/VTLL.h
  ${INCLUDE}/IntType.h
  ${INCLUDE}/VECSUtil.h
  ${INCLUDE}/VLLT.h
  ${INCLUDE}/VECS.h
  ${INCLUDE}/VECSIterator.h
)

add_executable(${TARGET} ${SOURCE} ${HEADERS})

target_compile_features(${TARGET} PUBLIC cxx_std_20)
