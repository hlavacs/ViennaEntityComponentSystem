
set(TARGET testvecs)

set(SOURCE testvecs.cpp testvecs_vecs.cpp)

set(HEADERS
  ${INCLUDE}/VECS.h
  ${INCLUDE}/VECSArchetype.h
  ${INCLUDE}/VECSHandle.h
  ${INCLUDE}/VECSMutex.h
  ${INCLUDE}/VECSSlotMap.h
  ${INCLUDE}/VECSVector.h
)

add_executable(${TARGET} ${SOURCE} ${HEADERS})

include_directories (${INCLUDE})
