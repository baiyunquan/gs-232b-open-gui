cmake_minimum_required(VERSION 3.21)

project(gs_232b_open_gui VERSION 0.1.0 LANGUAGES CXX)

include(CTest)

find_package(Qt6 6.5 REQUIRED COMPONENTS Core Widgets LinguistTools SerialPort WebSockets)

qt_standard_project_setup()

if(WIN32)
    set(VIRTUAL_SERIAL_PLATFORM_SOURCE virtual_serial/VirtualSerialEndpoint_win.cpp)
elseif(UNIX)
    set(VIRTUAL_SERIAL_PLATFORM_SOURCE virtual_serial/VirtualSerialEndpoint_posix.cpp)
else()
    message(FATAL_ERROR "Virtual serial is supported on Windows, macOS, and Linux")
endif()

qt_add_library(gs232b_virtual_serial STATIC
    virtual_serial/CrDelimitedBuffer.cpp
    virtual_serial/CrDelimitedBuffer.h
    virtual_serial/VirtualSerialBridge.cpp
    virtual_serial/VirtualSerialBridge.h
    virtual_serial/VirtualSerialEndpoint.h
    ${VIRTUAL_SERIAL_PLATFORM_SOURCE}
)
target_compile_features(gs232b_virtual_serial PUBLIC cxx_std_17)
target_include_directories(gs232b_virtual_serial PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(gs232b_virtual_serial PUBLIC Qt6::Core)
if(WIN32)
    target_link_libraries(gs232b_virtual_serial PRIVATE Qt6::SerialPort)
endif()

qt_add_executable(gs_232b_open_gui
    main.cpp
    CommunicationProtocol.cpp
    CommunicationProtocol.h
    MainWindow.cpp
    MainWindow.h
    ConnectionPanel.cpp
    ConnectionPanel.h
    ControlPanel.cpp
    ControlPanel.h
    Compass.cpp
    Compass.h
    ConsolePanel.cpp
    ConsolePanel.h
    RotorSettingsPanel.cpp
    RotorSettingsPanel.h
    WifiProvisioningDialog.cpp
    WifiProvisioningDialog.h
    ManagementKeyDialog.cpp
    ManagementKeyDialog.h
    FirmwareUpgradeDialog.cpp
    FirmwareUpgradeDialog.h
)

target_compile_features(gs_232b_open_gui PRIVATE cxx_std_17)

target_link_libraries(gs_232b_open_gui
    PRIVATE
        Qt6::Widgets
        Qt6::SerialPort
        Qt6::WebSockets
)

if(BUILD_TESTING)
    find_package(Qt6 6.5 REQUIRED COMPONENTS Test)
    qt_add_executable(virtual_serial_tests
        tests/VirtualSerialTests.cpp
    )
    target_compile_features(virtual_serial_tests PRIVATE cxx_std_17)
    target_link_libraries(virtual_serial_tests
        PRIVATE
            gs232b_virtual_serial
            Qt6::Test
    )
    add_test(NAME virtual_serial_tests COMMAND virtual_serial_tests)
endif()

qt_add_translations(gs_232b_open_gui
    TS_FILES translations/gs_232b_open_gui_zh_CN.ts
    RESOURCE_PREFIX "/i18n"
)
