if(TARGET MQT::QMapHybrid)
  file(COPY ${PROJECT_SOURCE_DIR}/test/hybridmap/architectures
       DESTINATION ${CMAKE_CURRENT_BINARY_DIR})
  file(COPY ${PROJECT_SOURCE_DIR}/test/hybridmap/circuits DESTINATION ${CMAKE_CURRENT_BINARY_DIR})

  file(GLOB_RECURSE HYBRIDMAP_TEST_SOURCES *.cpp)
  package_add_test_with_working_dir(${PROJECT_NAME}-hybridmap-test MQT::QMapHybrid
                                    ${CMAKE_CURRENT_BINARY_DIR} ${HYBRIDMAP_TEST_SOURCES})
  target_link_libraries(${PROJECT_NAME}-hybridmap-test PRIVATE MQT::CoreQASM)
endif()
