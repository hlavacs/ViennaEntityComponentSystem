##########################################################################
# Prerequisites for Console:
##########################################################################

# IMGui - fetch from Github into _deps

FetchContent_Declare(imgui
                     GIT_REPOSITORY https://github.com/ocornut/imgui.git
                     GIT_TAG docking)

FetchContent_MakeAvailable(imgui)

# SDL3 - fetch from Github into _deps

FetchContent_Declare(sdl3
                     GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
                     GIT_TAG main)

FetchContent_MakeAvailable(sdl3)

# IMGui - fetch from Github into _deps

FetchContent_Declare(implot
                     GIT_REPOSITORY https://github.com/epezent/implot.git
                     GIT_TAG master)

FetchContent_MakeAvailable(implot)

# Vulkan - the Vulkan SDK must have been installed properly
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
	ConsoleRegistry.cpp
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
  ${PROJECT_SOURCE_DIR}/include/VECS.h
  ${PROJECT_SOURCE_DIR}/include/VECSArchetype.h
  ${PROJECT_SOURCE_DIR}/include/VECSHandle.h
  ${PROJECT_SOURCE_DIR}/include/VECSMutex.h
  ${PROJECT_SOURCE_DIR}/include/VECSRegistry.h
  ${PROJECT_SOURCE_DIR}/include/VECSSlotMap.h
  ${PROJECT_SOURCE_DIR}/include/VECSVector.h
  ${PROJECT_SOURCE_DIR}/include/VECSConsoleComm.h
)

add_executable (${TARGET} ${SOURCE} ${HEADERS})

target_include_directories (${TARGET} PRIVATE ${imgui_SOURCE_DIR} ${imgui_SOURCE_DIR}/backends)
target_include_directories (${TARGET} PRIVATE ${sdl3_SOURCE_DIR}/include)
target_include_directories (${TARGET} PRIVATE ${implot_SOURCE_DIR})

target_link_libraries(${TARGET} Vulkan::Vulkan)
target_link_libraries(${TARGET} SDL3::SDL3)
target_link_libraries(${TARGET} ws2_32)