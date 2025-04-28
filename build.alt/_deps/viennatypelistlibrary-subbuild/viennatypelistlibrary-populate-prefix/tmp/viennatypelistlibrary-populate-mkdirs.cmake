# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file Copyright.txt or https://cmake.org/licensing for details.

cmake_minimum_required(VERSION 3.5)

# If CMAKE_DISABLE_SOURCE_CHANGES is set to true and the source directory is an
# existing directory in our source tree, calling file(MAKE_DIRECTORY) on it
# would cause a fatal error, even though it would be a no-op.
if(NOT EXISTS "D:/001_Uni/001_2025_BACHELORARBEIT/VECS/ViennaEntityComponentSystem/build/_deps/viennatypelistlibrary-src")
  file(MAKE_DIRECTORY "D:/001_Uni/001_2025_BACHELORARBEIT/VECS/ViennaEntityComponentSystem/build/_deps/viennatypelistlibrary-src")
endif()
file(MAKE_DIRECTORY
  "D:/001_Uni/001_2025_BACHELORARBEIT/VECS/ViennaEntityComponentSystem/build/_deps/viennatypelistlibrary-build"
  "D:/001_Uni/001_2025_BACHELORARBEIT/VECS/ViennaEntityComponentSystem/build/_deps/viennatypelistlibrary-subbuild/viennatypelistlibrary-populate-prefix"
  "D:/001_Uni/001_2025_BACHELORARBEIT/VECS/ViennaEntityComponentSystem/build/_deps/viennatypelistlibrary-subbuild/viennatypelistlibrary-populate-prefix/tmp"
  "D:/001_Uni/001_2025_BACHELORARBEIT/VECS/ViennaEntityComponentSystem/build/_deps/viennatypelistlibrary-subbuild/viennatypelistlibrary-populate-prefix/src/viennatypelistlibrary-populate-stamp"
  "D:/001_Uni/001_2025_BACHELORARBEIT/VECS/ViennaEntityComponentSystem/build/_deps/viennatypelistlibrary-subbuild/viennatypelistlibrary-populate-prefix/src"
  "D:/001_Uni/001_2025_BACHELORARBEIT/VECS/ViennaEntityComponentSystem/build/_deps/viennatypelistlibrary-subbuild/viennatypelistlibrary-populate-prefix/src/viennatypelistlibrary-populate-stamp"
)

set(configSubDirs Debug)
foreach(subDir IN LISTS configSubDirs)
    file(MAKE_DIRECTORY "D:/001_Uni/001_2025_BACHELORARBEIT/VECS/ViennaEntityComponentSystem/build/_deps/viennatypelistlibrary-subbuild/viennatypelistlibrary-populate-prefix/src/viennatypelistlibrary-populate-stamp/${subDir}")
endforeach()
if(cfgdir)
  file(MAKE_DIRECTORY "D:/001_Uni/001_2025_BACHELORARBEIT/VECS/ViennaEntityComponentSystem/build/_deps/viennatypelistlibrary-subbuild/viennatypelistlibrary-populate-prefix/src/viennatypelistlibrary-populate-stamp${cfgdir}") # cfgdir has leading slash
endif()
