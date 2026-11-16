cmake_minimum_required(VERSION 3.21)

project(gs_232b_open_gui VERSION 0.1.0 LANGUAGES C CXX)

if(WIN32)
    if(NOT MSVC)
        message(FATAL_ERROR "Windows builds require MSVC and the Qt msvc2022_64 SDK; use scripts/build-windows.sh")
    endif()
    # NOTE: 与 Qt 共享动态 CRT，避免跨模块分配和释放内存时使用不同的堆。
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")
    add_compile_options(/utf-8)
endif()

include(CTest)

find_package(Qt6 6.5 REQUIRED COMPONENTS Core Widgets LinguistTools SerialPort WebSockets)

qt_standard_project_setup()

include(cmake/TerminalDependencies.cmake)
qt_add_library(gs232b_terminal STATIC
    terminal/CommandLine.cpp
    terminal/CommandLine.h
    terminal/TerminalWidget.cpp
    terminal/TerminalWidget.h
    terminal/TerminalOutputBuffer.cpp
    terminal/TerminalOutputBuffer.h
)
target_compile_features(gs232b_terminal PUBLIC cxx_std_17)
target_include_directories(gs232b_terminal PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(gs232b_terminal PUBLIC qtpyte::core)

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

set(GUI_SOURCES
    AppSettings.cpp
    AppSettings.h
    AngleReading.cpp
    AngleReading.h
    CommandLog.cpp
    CommandLog.h
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
    StationGridDialog.cpp
    StationGridDialog.h
    FaultStatusMonitor.cpp
    FaultStatusMonitor.h
    FaultStatusPanel.cpp
    FaultStatusPanel.h
    WifiProvisioningDialog.cpp
    WifiProvisioningDialog.h
    ManagementKeyDialog.cpp
    ManagementKeyDialog.h
    PasswordChangeDialog.cpp
    PasswordChangeDialog.h
    NetworkPortConfigDialog.cpp
    NetworkPortConfigDialog.h
    FirmwareUpgradeDialog.cpp
    FirmwareUpgradeDialog.h
)

qt_add_executable(gs_232b_open_gui main.cpp ${GUI_SOURCES})

target_compile_features(gs_232b_open_gui PRIVATE cxx_std_17)

target_link_libraries(gs_232b_open_gui
    PRIVATE
        Qt6::Widgets
        Qt6::SerialPort
        Qt6::WebSockets
        gs232b_virtual_serial
        gs232b_terminal
)

if(BUILD_TESTING)
    find_package(Qt6 6.5 REQUIRED COMPONENTS Test)
    qt_add_executable(app_settings_tests
        tests/AppSettingsTests.cpp
        AppSettings.cpp
        AppSettings.h
    )
    target_compile_features(app_settings_tests PRIVATE cxx_std_17)
    target_include_directories(app_settings_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    target_link_libraries(app_settings_tests PRIVATE Qt6::Core Qt6::Test)
    add_test(NAME app_settings_tests COMMAND app_settings_tests)

    qt_add_executable(terminal_tests
        tests/TerminalTests.cpp
        ConsolePanel.cpp
        ConsolePanel.h
        CommandLog.cpp
        CommandLog.h
    )
    target_link_libraries(terminal_tests PRIVATE gs232b_terminal Qt6::Test)
    add_test(NAME terminal_tests COMMAND terminal_tests)
    set_tests_properties(terminal_tests PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

    qt_add_executable(command_log_tests
        tests/CommandLogTests.cpp
        CommandLog.cpp
        CommandLog.h
    )
    target_compile_features(command_log_tests PRIVATE cxx_std_17)
    target_include_directories(command_log_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    target_link_libraries(command_log_tests PRIVATE Qt6::Core Qt6::Test)
    add_test(NAME command_log_tests COMMAND command_log_tests)

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

    qt_add_executable(fault_status_tests
        tests/FaultStatusMonitorTests.cpp
        FaultStatusMonitor.cpp
        FaultStatusMonitor.h
    )
    target_compile_features(fault_status_tests PRIVATE cxx_std_17)
    target_include_directories(fault_status_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    target_link_libraries(fault_status_tests PRIVATE Qt6::Core Qt6::Test)
    add_test(NAME fault_status_tests COMMAND fault_status_tests)

    qt_add_executable(rotor_model_connection_tests
        tests/RotorModelConnectionTests.cpp
        ${GUI_SOURCES}
    )
    target_compile_features(rotor_model_connection_tests PRIVATE cxx_std_17)
    target_include_directories(rotor_model_connection_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    target_link_libraries(rotor_model_connection_tests
        PRIVATE Qt6::Widgets Qt6::SerialPort Qt6::WebSockets Qt6::Test gs232b_virtual_serial gs232b_terminal
    )
    add_test(NAME rotor_model_connection_tests COMMAND rotor_model_connection_tests)
    set_tests_properties(rotor_model_connection_tests PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
    )

    qt_add_executable(ap_provisioning_connection_tests
        tests/ApProvisioningConnectionTests.cpp
        ${GUI_SOURCES}
    )
    target_compile_features(ap_provisioning_connection_tests PRIVATE cxx_std_17)
    target_include_directories(ap_provisioning_connection_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    target_link_libraries(ap_provisioning_connection_tests
        PRIVATE Qt6::Widgets Qt6::SerialPort Qt6::WebSockets Qt6::Test gs232b_virtual_serial gs232b_terminal
    )
    add_test(NAME ap_provisioning_connection_tests COMMAND ap_provisioning_connection_tests)
    set_tests_properties(ap_provisioning_connection_tests PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
    )

    qt_add_executable(virtual_serial_connection_tests
        tests/VirtualSerialConnectionTests.cpp
        ${GUI_SOURCES}
    )
    target_compile_features(virtual_serial_connection_tests PRIVATE cxx_std_17)
    target_include_directories(virtual_serial_connection_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    target_link_libraries(virtual_serial_connection_tests
        PRIVATE Qt6::Widgets Qt6::SerialPort Qt6::WebSockets Qt6::Test gs232b_virtual_serial gs232b_terminal
    )
    add_test(NAME virtual_serial_connection_tests COMMAND virtual_serial_connection_tests)
    set_tests_properties(virtual_serial_connection_tests PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
    )
endif()

qt_add_translations(gs_232b_open_gui
    TS_FILES translations/gs_232b_open_gui_zh_CN.ts
    RESOURCE_PREFIX "/i18n"
)
