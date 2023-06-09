include(FlightGearComponent)

set(SOURCES
	AIAircraft.cxx
	AIBallistic.cxx
	AIBase.cxx
	AIBaseAircraft.cxx
	AICarrier.cxx
	AIEscort.cxx
	AIFlightPlan.cxx
	AIFlightPlanCreate.cxx
	AIFlightPlanCreateCruise.cxx
	AIFlightPlanCreatePushBack.cxx
	AIGroundVehicle.cxx
	AIManager.cxx
	AIMultiplayer.cxx
	AIShip.cxx
	AIStatic.cxx
	AIStorm.cxx
	AITanker.cxx
	AIThermal.cxx
	AIWingman.cxx
	performancedata.cxx
	performancedb.cxx
	submodel.cxx
	VectorMath.cxx
	)

set(HEADERS
	AIAircraft.hxx
	AIBallistic.hxx
	AIBase.hxx
	AIBaseAircraft.hxx
	AICarrier.hxx
	AIEscort.hxx
	AIFlightPlan.hxx
	AIGroundVehicle.hxx
	AIManager.hxx
	AIMultiplayer.hxx
	AINotifications.hxx
	AIShip.hxx
	AIStatic.hxx
	AIStorm.hxx
	AITanker.hxx
	AIThermal.hxx
	AIWingman.hxx
	performancedata.hxx
	performancedb.hxx
	submodel.hxx
	VectorMath.cxx
	)

# Add sources if swift support is enabled
if(ENABLE_SWIFT)
	list(APPEND HEADERS AISwiftAircraft.h)
	list(APPEND SOURCES AISwiftAircraft.cpp)
endif()

flightgear_component(AIModel "${SOURCES}" "${HEADERS}")
