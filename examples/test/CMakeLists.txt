
set(TARGET test)

set(SOURCE test.cpp)

set(HEADERS
  test.h
  ${INCLUDE}/VTLL.h
  ${INCLUDE}/VECSUtil.h
  ${INCLUDE}/VECSContainer.h
  ${INCLUDE}/VECSContainer1.h
  ${INCLUDE}/VECSComponent.h
  ${INCLUDE}/VECS.h
  ${INCLUDE}/VECSUser.h
)

add_executable(${TARGET} ${SOURCE} ${HEADERS})

target_compile_features(${TARGET} PUBLIC cxx_std_20)
