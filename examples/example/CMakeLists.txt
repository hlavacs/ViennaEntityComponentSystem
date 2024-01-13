
set(TARGET example)

set(SOURCE example.cpp)

set(HEADERS
  example.h
  ${PROJECT_SOURCE_DIR}/include/VECS.h
  ${PROJECT_SOURCE_DIR}/extern/ViennaTypeListLibrary/VTLL.h
  ${PROJECT_SOURCE_DIR}/extern/ViennaStrongType/VSTY.h
  ${PROJECT_SOURCE_DIR}/extern/ViennaLockLessTable/VLLT.h
)

add_executable(${TARGET} ${SOURCE} ${HEADERS})

target_include_directories(${TARGET} PUBLIC ${PROJECT_SOURCE_DIR}/include)
target_include_directories(${TARGET} PUBLIC ${PROJECT_SOURCE_DIR}/extern/ViennaTypeListLibrary)
target_include_directories(${TARGET} PUBLIC ${PROJECT_SOURCE_DIR}/extern/ViennaStrongType)
target_include_directories(${TARGET} PUBLIC ${PROJECT_SOURCE_DIR}/extern/ViennaLockLessTable)


target_compile_features(${TARGET} PUBLIC cxx_std_20)
