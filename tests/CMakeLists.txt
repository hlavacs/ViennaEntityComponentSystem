
set(TARGET testvecs)

set(SOURCE testvecs.cpp testvecs_vecs.cpp)

set(HEADERS
  ${PROJECT_SOURCE_DIR}/include/VECS.h
  ${PROJECT_SOURCE_DIR}/include/VECSArchetype.h
  ${PROJECT_SOURCE_DIR}/include/VECSHandle.h
  ${PROJECT_SOURCE_DIR}/include/VECSMutex.h
  ${PROJECT_SOURCE_DIR}/include/VECSSlotMap.h
  ${PROJECT_SOURCE_DIR}/include/VECSVector.h
)

add_executable(${TARGET} ${SOURCE} ${HEADERS})

