##########################################################################
# Prerequisites for Console:
##########################################################################

# Debian prerequisites:
# sudo apt install -y build-essential automake cmake git libsdl3-dev libvulkan-dev
# might additionally be necessary: libvulkan1 mesa-vulkan-drivers

# IMGui - fetch from Github into _deps

FetchContent_Declare(imgui
                     GIT_REPOSITORY https://github.com/ocornut/imgui.git
                     GIT_TAG docking)

FetchContent_MakeAvailable(imgui)

# SDL3

# use same settings as VVE:
set(SDL_STATIC ON)
set(SDL_SHARED OFF)
FetchContent_Declare(sdl3
					GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
                    GIT_TAG main
)
FetchContent_MakeAvailable(sdl3)
include_directories (${sdl3_SOURCE_DIR}/include)
include_directories (${sdl3_SOURCE_DIR}/include/SDL3)
# That one's not in VVE:
set(SDL3_LIB SDL3::SDL3)

## Detect platform
#if(WIN32)
## on Windows, fetch from Github into _deps
#    message(STATUS "Configuring SDL3 via FetchContent for Windows")
#
#FetchContent_Declare(sdl3
#                     GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
#                     GIT_TAG main)
#
#FetchContent_MakeAvailable(sdl3)
#set(SDL3_LIB SDL3::SDL3)
#elseif(UNIX AND NOT APPLE)
## on Linux, use pre-installed package
#    message(STATUS "Using system-installed SDL3 on Linux")
#
#    find_package(SDL3 REQUIRED CONFIG)
#    if(NOT SDL3_FOUND)
#        message(FATAL_ERROR "SDL3 not found. Please install libsdl3-dev or equivalent.")
#    endif()
#    set(SDL3_LIB SDL3::SDL3)
#endif()


# IMGui - fetch from Github into _deps

FetchContent_Declare(implot
                     GIT_REPOSITORY https://github.com/epezent/implot.git
                     GIT_TAG master)

FetchContent_MakeAvailable(implot)

# Vulkan - the Vulkan SDK must have been installed properly
message(STATUS "Using system-installed Vulkan SDK")

find_package(Vulkan REQUIRED)

set(VULKAN $ENV{VULKAN_SDK})

include_directories (${Vulkan_INCLUDE_DIRS})


##########################################################################
# Console:
##########################################################################

set(TARGET Console)

set(SOURCE
    Console.cpp
	Listener.cpp
	ConsoleListener.cpp
	ImguiInstance.cpp
	ConsoleArchetype.cpp
	${imgui_SOURCE_DIR}/imgui.cpp
	${imgui_SOURCE_DIR}/imgui_draw.cpp
	${imgui_SOURCE_DIR}/imgui_tables.cpp
	${imgui_SOURCE_DIR}/imgui_widgets.cpp
	${imgui_SOURCE_DIR}/backends/imgui_impl_vulkan.cpp
	${imgui_SOURCE_DIR}/backends/imgui_impl_sdl3.cpp
	${implot_SOURCE_DIR}/implot.cpp
	${implot_SOURCE_DIR}/implot_items.cpp)


# VECS headers are maybe unnecessary in the console?
set(HEADERS
  Listener.h
  Console.h
  ConsoleListener.h
  ConsoleArchetype.h
  ConsoleEntity.h
  ConsoleComponent.h
  ConsoleRegistry.h
  ${implot_SOURCE_DIR}/implot.h
  ${implot_SOURCE_DIR}/implot_internal.h
)

add_executable (${TARGET} ${SOURCE} ${HEADERS})

target_include_directories (${TARGET} PRIVATE ${imgui_SOURCE_DIR} ${imgui_SOURCE_DIR}/backends)
target_include_directories (${TARGET} PRIVATE ${sdl3_SOURCE_DIR}/include)
target_include_directories (${TARGET} PRIVATE ${implot_SOURCE_DIR})

target_link_libraries(${TARGET} Vulkan::Vulkan)
target_link_libraries(${TARGET} ${SDL3_LIB})
if(WIN32)
target_link_libraries(${TARGET} ws2_32)
endif()
