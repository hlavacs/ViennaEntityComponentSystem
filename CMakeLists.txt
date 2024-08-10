
cmake_minimum_required (VERSION 3.5.0)
project (  
  "ViennaEntittyCOmpnentSystem"
  VERSION 2.0.0
  DESCRIPTION "An entity component system for game engines"
  LANGUAGES CXX
)

# enforce C++20 standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if (MSVC)
  set(CMAKE_CXX_FLAGS_RELEASE /O2)
  add_compile_options(/await)
  set(CMAKE_VS_PLATFORM_NAME "x64")
  add_compile_options(/EHsc)
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "Clang" OR CMAKE_CXX_COMPILER_ID STREQUAL "AppleClang")
  add_compile_options(-stdlib=libc++)
  add_compile_options(-pthread)
  add_compile_options(-fcoroutines-ts)
elseif(CMAKE_COMPILER_IS_GNUCXX)
endif()

set(INCLUDE ${CMAKE_HOME_DIRECTORY}/include)
set(EXTERN ${CMAKE_HOME_DIRECTORY}/extern)

include_directories (${EXTERN}/ViennaTypeListLibrary/include)



# Testing

include(CTest)

add_subdirectory(tests)
