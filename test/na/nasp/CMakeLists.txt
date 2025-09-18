if(TARGET MQT::NASP)
  file(GLOB NA_TEST_SOURCES *.cpp)
  package_add_test(mqt-qmap-nasp-test MQT::NASP ${NA_TEST_SOURCES})
  target_compile_definitions(mqt-qmap-nasp-test
                             PRIVATE TEST_CIRCUITS_PATH="${CMAKE_CURRENT_SOURCE_DIR}/circuits")
  target_link_libraries(mqt-qmap-nasp-test PRIVATE MQT::CoreQASM)
endif()
