include(FlightGearComponent)

set(SOURCES
	apt_loader.cxx
	dynamicloader.cxx
	dynamics.cxx
	gnnode.cxx
	groundnetwork.cxx
	parking.cxx
	pavement.cxx
	runwaybase.cxx
	runwayprefloader.cxx
	runwayprefs.cxx
	runways.cxx
	sidstar.cxx
	airport.cxx
	xmlloader.cxx
	airportdynamicsmanager.cxx
	AirportBuilder.cxx
	)

set(HEADERS
	airports_fwd.hxx
	apt_loader.hxx
	dynamicloader.hxx
	dynamics.hxx
	gnnode.hxx
	groundnetwork.hxx
	parking.hxx
	pavement.hxx
	runwaybase.hxx
	runwayprefloader.hxx
	runwayprefs.hxx
	runways.hxx
	sidstar.hxx
	airport.hxx
	xmlloader.hxx
	airportdynamicsmanager.hxx
	AirportBuilder.hxx
	)

flightgear_component(Airports "${SOURCES}" "${HEADERS}")

include(SetupFGFSIncludes)
include(SetupFGFSLibraries)

# 设置变量
SET(LIBAPT_SRC ./apt_loader_wrapper.h ./apt_loader_wrapper.cpp)
SET(CMAKE_LIBRARY_OUTPUT_DIRECTORY ./)

# 第一个参数为你需要构建的dll的名字，第二个为类型
ADD_LIBRARY(apt_dll SHARED ${LIBAPT_SRC} $<TARGET_OBJECTS:fgfsObjects> $<TARGET_OBJECTS:fgfs>)
add_dependencies(apt_dll fgfsObjects)
add_dependencies(apt_dll fgfs)

# Set up the target links.
setup_fgfs_libraries(apt_dll)
export_debug_symbols(apt_dll)
# Additional search paths for includes.
setup_fgfs_includes(apt_dll)

INSTALL(TARGETS apt_dll)
# 为dll设置linker
# 指定dll的生成目录，这里是：./
#target_link_libraries(apt_dll fgfsObjects)
SET_TARGET_PROPERTIES(apt_dll PROPERTIES LINKER_LANGUAGE C
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_LIBRARY_OUTPUT_DIRECTORY}
        LIBRARY_OUTPUT_DIRECTORY ${CMAKE_LIBRARY_OUTPUT_DIRECTORY}
        OUTPUT_NAME "apt_loader"
        PREFIX "")