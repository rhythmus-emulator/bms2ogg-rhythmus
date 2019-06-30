project(rencoder_bin)

include_directories(
    ${RENCODER_INCLUDE_DIR}
    ${RPARSER_INCLUDE_DIR}
	)

set(RENCODER_BIN_SOURCE
    main.cpp
    rencoder.cpp
    )
    
set(RENCODER_BIN_HEADER
    rencoder.h
    )

add_executable(rencoder_bin ${RENCODER_BIN_SOURCE} ${RENCODER_BIN_HEADER})

target_link_libraries(rencoder_bin
    rencoder_lib
    rparser
    timidity
    ${OPENSSL_LIBRARY}
    ${ZIP_LIBRARY}
    ${ZLIB_LIBRARY}
    )

# To enable GUI interface encoder
# use -DENCODERGUI=true

if (GUI)
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
    
    set(RENCODER_GUI_SOURCE
        main_gui.cpp
        rencoder.cpp
        )
        
    set(RENCODER_GUI_HEADER
        rencoder.h
        )

    add_executable(rencoder_gui_bin ${RENCODER_GUI_SOURCE} ${RENCODER_GUI_HEADER})
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
