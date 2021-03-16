
set(TARGET parallel)

set(SOURCE parallel.cpp)

set(HEADERS
  parallel.h
  ${INCLUDE}/IntType.h
  VGJS.h
  VGJSCoro.h
  ${INCLUDE}/VTLL.h
  ${INCLUDE}/VECSUtil.h
  ${INCLUDE}/VECSTable.h
  ${INCLUDE}/VECSCompSystem.h
  ${INCLUDE}/VECSCompUser.h
  ${INCLUDE}/VECS.h
)

add_executable(${TARGET} ${SOURCE} ${HEADERS})

target_compile_features(${TARGET} PUBLIC cxx_std_20)
