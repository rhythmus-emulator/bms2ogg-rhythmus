project(rencoder_bin)

include_directories(
    ${RENCODER_INCLUDE_DIR}
    ${RPARSER_INCLUDE_DIR}
	)

add_executable(rencoder_bin "main.cpp")

target_link_libraries(rencoder_bin rencoder_lib)

# To enable GUI interface encoder
# use -DENCODERGUI=true

if (ENCODERGUI)
    if (EXISTS ${PROJECT_SOURCE_DIR}/../../wxWidgets-lib)
        set(CMAKE_MODULE_PATH "${CMAKE_MODULE_PATH};${PROJECT_SOURCE_DIR}/../../wxWidgets-lib")
        message("wxWidgets-lib found. Use precompiled library.")
    else()
        message("No wxWidgets-lib found. Use system library.")
    endif()

    find_package(wxWidgets)
    if (NOT wxWidgets_FOUND)
        message(FATAL_ERROR "wxWidgets not found. wxWidgets required.")
    endif()
    
    add_executable(rencoder_gui_bin "main_gui.cpp")
    target_link_libraries(rencoder_gui_bin
        rencoder_lib
        ${wxWidgets_LIBRARIES}
        "comctl32.lib"
        "rpcrt4.lib"
        )
    target_include_directories(rencoder_gui_bin PUBLIC ${wxWidgets_INCLUDE_DIRS})

    # not working... you may need to reset SUBSYSTEM for GUI project ...
    #target_compile_options(rencoder_gui_bin PRIVATE "/SUBSYSTEM:WINDOWS")
endif()
