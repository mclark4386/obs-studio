project(libobs-d3d11)

include_directories(SYSTEM "${CMAKE_SOURCE_DIR}/libobs")

find_package(DirectX REQUIRED)
if(NOT DirectX_D3D11_FOUND)
	message(FATAL_ERROR "No D3D11 SDK Found!")
endif()
include_directories(
	${DirectX_D3D11_INCLUDE_DIRS})

add_definitions(-DLIBOBS_EXPORTS)

set(libobs-d3d11_SOURCES
	d3d11-indexbuffer.cpp
	d3d11-samplerstate.cpp
	d3d11-shader.cpp
	d3d11-shaderprocessor.cpp
	d3d11-stagesurf.cpp
	d3d11-subsystem.cpp
	d3d11-texture2d.cpp
	d3d11-vertexbuffer.cpp
	d3d11-zstencilbuffer.cpp)

set(libobs-d3d11_HEADERS
	d3d11-exports.h
	d3d11-shaderprocessor.hpp
	d3d11-subsystem.hpp)
	
add_library(libobs-d3d11 MODULE
	${libobs-d3d11_SOURCES}
	${libobs-d3d11_HEADERS})
set_target_properties(libobs-d3d11
	PROPERTIES
		OUTPUT_NAME libobs-d3d11
		PREFIX "")
target_link_libraries(libobs-d3d11
	libobs
	${DirectX_LIBRARY}
	${DirectX_DXGI_LIBRARY}
	${DirectX_D3DCOMPILER_LIBRARY}
	${DirectX_D3D11_LIBRARIES})

install_obs_core(libobs-d3d11)
