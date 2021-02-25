
set(TARGET test)

set(SOURCE test.cpp)

set(HEADERS
  test.h
  ${INCLUDE}/VTLL.h
  ${INCLUDE}/IntType.h
  ${INCLUDE}/VECSMutex.h
  ${INCLUDE}/VECSUtil.h
  ${INCLUDE}/VECSTable.h
  ${INCLUDE}/VECSCompSystem.h
  ${INCLUDE}/VECSCompUser.h
  ${INCLUDE}/VECS.h
)

add_executable(${TARGET} ${SOURCE} ${HEADERS})

target_compile_features(${TARGET} PUBLIC cxx_std_20)
