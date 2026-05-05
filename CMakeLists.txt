cmake_minimum_required(VERSION 3.20)

project(SerialParser VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(Qt6 REQUIRED COMPONENTS Widgets SerialPort Charts)

qt_standard_project_setup()

set(SERIALPARSER_APP_SOURCES
    main.cpp
    MainWindow.h
    MainWindow.cpp
    SerialService.h
    SerialService.cpp
    ProtocolConfig.h
    ProtocolConfig.cpp
    ProtocolParser.h
    ProtocolParser.cpp
    ConfigManager.h
    ConfigManager.cpp
    CurvePanel.h
    CurvePanel.cpp
    CrcUtil.h
    CrcUtil.cpp
    HexUtil.h
    HexUtil.cpp
    EncodingUtil.h
    EncodingUtil.cpp
    resources/resources.qrc
)

if(WIN32)
    list(APPEND SERIALPARSER_APP_SOURCES resources/app_icon.rc)
endif()

qt_add_executable(SerialParserApp
    ${SERIALPARSER_APP_SOURCES}
)

target_link_libraries(SerialParserApp PRIVATE Qt6::Widgets Qt6::SerialPort Qt6::Charts)

set_target_properties(SerialParserApp PROPERTIES
    OUTPUT_NAME "SerialParserApp"
)

if(WIN32)
    # Release hides the console, Debug keeps it visible for diagnostics.
    set_target_properties(SerialParserApp PROPERTIES
        WIN32_EXECUTABLE "$<CONFIG:Release>"
    )
endif()

add_custom_command(TARGET SerialParserApp POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
            ${CMAKE_SOURCE_DIR}/configs
            $<TARGET_FILE_DIR:SerialParserApp>/configs
    COMMAND ${CMAKE_COMMAND} -E copy_directory
            ${CMAKE_SOURCE_DIR}/styles
            $<TARGET_FILE_DIR:SerialParserApp>/styles
    COMMAND ${CMAKE_COMMAND} -E copy_directory
            ${CMAKE_SOURCE_DIR}/resources
            $<TARGET_FILE_DIR:SerialParserApp>/resources
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${CMAKE_SOURCE_DIR}/README.md
            $<TARGET_FILE_DIR:SerialParserApp>/README.md
)

if(WIN32)
    add_executable(SerialParser WIN32
        launcher/SerialParserLauncher.cpp
        resources/app_icon.rc
    )
endif()
