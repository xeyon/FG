

# note order here affects link order, and hence linking correctness
# on systems with a traditional ld (eg, GNU ld on Linux)
foreach( mylibfolder
		Add-ons
		Airports
		Aircraft
		ATC
		EmbeddedResources
		Canvas
		Radio
		Autopilot
		Cockpit
		Environment
		GUI
		Input
		Instrumentation
		Model
		MultiPlayer
		AIModel
		Navaids
		Network
		Scenery
		Scripting
		Sound
		Systems
		Time
		Traffic
		FDM
		Viewer
		Main
	)

	add_subdirectory(${mylibfolder})

endforeach( mylibfolder )

