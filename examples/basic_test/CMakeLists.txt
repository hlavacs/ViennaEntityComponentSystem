
set(TARGET basic_test)

set(SOURCE basic_test.cpp)

set(HEADERS
  basic_test.h
  ${INCLUDE}/VTLL.h
  ${INCLUDE}/VECSUtil.h
  ${INCLUDE}/VECSTable.h
  ${INCLUDE}/VECSContainer1.h
  ${INCLUDE}/VECSComponent.h
  ${INCLUDE}/VECS.h
  ${INCLUDE}/VECSUser.h
)

add_executable(${TARGET} ${SOURCE} ${HEADERS})

target_compile_features(${TARGET} PUBLIC cxx_std_20)
