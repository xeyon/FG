set(TESTSUITE_SOURCES
        ${TESTSUITE_SOURCES}
        ${CMAKE_CURRENT_SOURCE_DIR}/TestSuite.cxx
        ${CMAKE_CURRENT_SOURCE_DIR}/test_swiftService.cxx
        ${CMAKE_CURRENT_SOURCE_DIR}/test_swiftAircraftManager.cxx
        PARENT_SCOPE
        )

set(TESTSUITE_HEADERS
        ${TESTSUITE_HEADERS}
        ${CMAKE_CURRENT_SOURCE_DIR}/test_swiftService.hxx
        ${CMAKE_CURRENT_SOURCE_DIR}/test_swiftAircraftManager.hxx
        PARENT_SCOPE
        )
