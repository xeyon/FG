include(FlightGearComponent)

set(SOURCES
	mqttd.cxx
	PropertyChangeObserver.cxx
	)

set(HEADERS
	mqttd.hxx
	PropertyChangeObserver.hxx
	)

flightgear_component(Mqtt "${SOURCES}" "${HEADERS}")
