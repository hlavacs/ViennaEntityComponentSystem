
set(TARGET testparallel)

set(SOURCE testparallel_manager.cpp)

set(HEADERS
  ${PROJECT_SOURCE_DIR}/include/VECS.h
  ${PROJECT_SOURCE_DIR}/include/VECSArchetype.h
  ${PROJECT_SOURCE_DIR}/include/VECSHandle.h
  ${PROJECT_SOURCE_DIR}/include/VECSMutex.h
  ${PROJECT_SOURCE_DIR}/include/VECSSlotMap.h
  ${PROJECT_SOURCE_DIR}/include/VECSVector.h
  ${PROJECT_SOURCE_DIR}/include/VECSManager.h
)

add_executable(${TARGET} ${SOURCE} ${HEADERS})


target_link_libraries(
  testparallel
  GTest::gtest_main
)

include(GoogleTest)

gtest_discover_tests(testparallel)