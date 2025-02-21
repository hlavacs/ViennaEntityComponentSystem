
set(TARGET testvecs)

set(SOURCE testvecs.cpp testvecs_vecs.cpp)

set(HEADERS
  ${INCLUDE}/VECS.h
  ${INCLUDE}/VECS2.h
  ${INCLUDE}/VECSArchetype.h
  ${INCLUDE}/VECSArchetype2.h
  ${INCLUDE}/VECSHandle.h
  ${INCLUDE}/VECSHashMap.h
  ${INCLUDE}/VECSMutex.h
  ${INCLUDE}/VECSSlotMap.h
  ${INCLUDE}/VECSVector.h
)

add_executable(${TARGET} ${SOURCE} ${HEADERS})

include_directories (${INCLUDE})
