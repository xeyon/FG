set(TESTSUITE_SOURCES
    ${TESTSUITE_SOURCES}
    ${CMAKE_CURRENT_SOURCE_DIR}/TestSuite.cxx
    ${CMAKE_CURRENT_SOURCE_DIR}/test_ls_matrix.cxx
    ${CMAKE_CURRENT_SOURCE_DIR}/testAeroElement.cxx
    ${CMAKE_CURRENT_SOURCE_DIR}/testYASimAtmosphere.cxx
    ${CMAKE_CURRENT_SOURCE_DIR}/testYASimGear.cxx
    PARENT_SCOPE
)

set(TESTSUITE_HEADERS
    ${TESTSUITE_HEADERS}
    ${CMAKE_CURRENT_SOURCE_DIR}/test_ls_matrix.hxx
    ${CMAKE_CURRENT_SOURCE_DIR}/testAeroElement.hxx
    ${CMAKE_CURRENT_SOURCE_DIR}/testYASimAtmosphere.hxx
    ${CMAKE_CURRENT_SOURCE_DIR}/testYASimGear.hxx
    PARENT_SCOPE
)
