
cmake_minimum_required (VERSION 3.5.0)
project (  
  "ViennaEntittyCOmpnentSystem"
  VERSION 2.0.0
  DESCRIPTION "An entity component system for game engines"
  LANGUAGES CXX
)

# enforce C++23 standard
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if (MSVC)
  set(CMAKE_CXX_FLAGS_RELEASE /O2)
  add_compile_options(/await)
  set(CMAKE_VS_PLATFORM_NAME "x64")
  add_compile_options(/EHsc)
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "Clang" OR CMAKE_CXX_COMPILER_ID STREQUAL "AppleClang")
  add_compile_options(-stdlib=libc++)
elseif(CMAKE_COMPILER_IS_GNUCXX)
endif()

set(INCLUDE ${PROJECT_SOURCE_DIR}/include)
set(EXTERN ${PROJECT_SOURCE_DIR}/extern)

include(FetchContent) # once in the project to include the module

FetchContent_Declare(ViennaStrongType
                     GIT_REPOSITORY https://github.com/hlavacs/ViennaStrongType.git 
                     GIT_TAG main)

FetchContent_MakeAvailable(ViennaStrongType)

include_directories (${ViennaStrongType_SOURCE_DIR}/include)

FetchContent_Declare(ViennaTypeListLibrary
                     GIT_REPOSITORY https://github.com/hlavacs/ViennaTypeListLibrary.git 
                     GIT_TAG main)

FetchContent_MakeAvailable(ViennaTypeListLibrary)

include_directories (${ViennaTypeListLibrary_SOURCE_DIR}/include)


# Testing

include(CTest)

add_subdirectory(tests)
