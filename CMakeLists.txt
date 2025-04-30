
cmake_minimum_required (VERSION 3.5.0)
project (  
  "ViennaEntityComponentSystem"
  VERSION 2.0.0
  DESCRIPTION "An entity component system for game engines"
  LANGUAGES CXX
)

# enforce C++23 standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if (CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
  set(CMAKE_CXX_FLAGS_RELEASE /O2)
  add_compile_options(/await)
  add_compile_options(/EHsc)
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "AppleClang")
  add_compile_options(-stdlib=libc++)
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
  add_compile_options(-stdlib=libc++)
  if(UNIX)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -stdlib=libc++ -v")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -stdlib=libc++ -lc++abi")
  endif()
elseif(CMAKE_COMPILER_IS_GNUCXX)
endif()

include_directories (${PROJECT_SOURCE_DIR}/include)

include(FetchContent) # once in the project to include the module

FetchContent_Declare(viennastrongtype
                     GIT_REPOSITORY https://github.com/hlavacs/ViennaStrongType.git 
                     GIT_TAG main)

FetchContent_MakeAvailable(viennastrongtype)

include_directories (${viennastrongtype_SOURCE_DIR}/include)

FetchContent_Declare(viennatypelistlibrary
                     GIT_REPOSITORY https://github.com/hlavacs/ViennaTypeListLibrary.git 
                     GIT_TAG main)

FetchContent_MakeAvailable(viennatypelistlibrary)

include_directories (${viennatypelistlibrary_SOURCE_DIR}/include)


# Testing

include(CTest)

add_subdirectory(tests)

# Console
add_subdirectory(Console)