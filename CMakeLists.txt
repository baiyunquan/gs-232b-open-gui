cmake_minimum_required(VERSION 3.21)

project(gs_232b_open_gui VERSION 0.1.0 LANGUAGES CXX)

find_package(Qt6 6.5 REQUIRED COMPONENTS Widgets LinguistTools)

qt_standard_project_setup()

qt_add_executable(gs_232b_open_gui
    main.cpp
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
)

qt_add_translations(gs_232b_open_gui
    TS_FILES translations/gs_232b_open_gui_zh_CN.ts
    RESOURCE_PREFIX "/i18n"
)
