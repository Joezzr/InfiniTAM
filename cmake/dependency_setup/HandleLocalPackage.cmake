function(handle_local_package)
	set(options)
    set(oneValueArgs NAME ALTERNATIVE_LOCAL_NAME REQUIRE_LOCAL_FOUND)
    set(multiValueArgs COMPONENTS OPTIONAL_COMPONENTS)
    cmake_parse_arguments(HLP "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})

    list(LENGTH HLP_COMPONENTS COMPONENTS_LENGTH)
    list(LENGTH HLP_OPTIONAL_COMPONENTS OPTIONAL_COMPONENTS_LENGTH)
    if(NOT HLP_ALTERNATIVE_LOCAL_NAME)
        set(HLP_ALTERNATIVE_LOCAL_NAME ${HLP_NAME})
    endif()
    if (HLP_REQUIRE_LOCAL_FOUND)
        if(COMPONENTS_LENGTH GREATER 0)
            if(OPTIONAL_COMPONENTS_LENGTH GREATER 0)
                find_package(${HLP_ALTERNATIVE_LOCAL_NAME} QUIET REQUIRED COMPONENTS ${HLP_COMPONENTS} OPTIONAL_COMPONENTS ${HLP_OPTIONAL_COMPONENTS})
            else()
                find_package(${HLP_ALTERNATIVE_LOCAL_NAME} QUIET REQUIRED COMPONENTS ${HLP_COMPONENTS})
            endif()
        else()
            find_package(${HLP_ALTERNATIVE_LOCAL_NAME} QUIET REQUIRED)
        endif()
    else ()
        if(COMPONENTS_LENGTH GREATER 0)
            if(OPTIONAL_COMPONENTS_LENGTH GREATER 0)
                find_package(${HLP_ALTERNATIVE_LOCAL_NAME} QUIET COMPONENTS ${HLP_COMPONENTS} OPTIONAL_COMPONENTS ${HLP_OPTIONAL_COMPONENTS})
            else()
                find_package(${HLP_ALTERNATIVE_LOCAL_NAME} QUIET COMPONENTS ${HLP_COMPONENTS} )
            endif()
        else()
            find_package(${HLP_ALTERNATIVE_LOCAL_NAME} QUIET)
        endif()
    endif ()
    if (${HLP_ALTERNATIVE_LOCAL_NAME}_FOUND)
        add_library(${HLP_NAME} INTERFACE IMPORTED GLOBAL)
        if(COMPONENTS_LENGTH GREATER 0)
            set(INTERFACE_INCLUDE_DIRECTORIES)
            foreach(COMPONENT ${HLP_COMPONENTS})
                target_link_libraries(${HLP_NAME} INTERFACE ${HLP_ALTERNATIVE_LOCAL_NAME}::${COMPONENT})
                get_target_property(${COMPONENT}_INTERFACE_INCLUDE_DIRECTORIES ${HLP_ALTERNATIVE_LOCAL_NAME}::${COMPONENT} INTERFACE_INCLUDE_DIRECTORIES)
                if(${COMPONENT}_INTERFACE_INCLUDE_DIRECTORIES)
                    list(APPEND INTERFACE_INCLUDE_DIRECTORIES ${${COMPONENT}_INTERFACE_INCLUDE_DIRECTORIES})
                endif()
            endforeach(COMPONENT)
            foreach(COMPONENT ${HLP_OPTIOHNAL_COMPONENTS})
                if(TARGET ${COMPONENT})
                    target_link_libraries(${HLP_NAME} INTERFACE ${HLP_ALTERNATIVE_LOCAL_NAME}::${COMPONENT})
                    get_target_property(${COMPONENT}_INTERFACE_INCLUDE_DIRECTORIES ${HLP_ALTERNATIVE_LOCAL_NAME}::${COMPONENT} INTERFACE_INCLUDE_DIRECTORIES)
                    if(${COMPONENT}_INTERFACE_INCLUDE_DIRECTORIES)
                        list(APPEND INTERFACE_INCLUDE_DIRECTORIES ${${COMPONENT}_INTERFACE_INCLUDE_DIRECTORIES})
                    endif()
                endif()
            endforeach(COMPONENT)
            if(INTERFACE_INCLUDE_DIRECTORIES)
                list(REMOVE_DUPLICATES INTERFACE_INCLUDE_DIRECTORIES)
                set_target_properties(${HLP_NAME} PROPERTIES INTERFACE_INCLUDE_DIRECTORIES ${INTERFACE_INCLUDE_DIRECTORIES})
            endif()
        else()
            target_link_libraries(${HLP_NAME} INTERFACE ${HLP_ALTERNATIVE_LOCAL_NAME}::${HLP_ALTERNATIVE_LOCAL_NAME})
        endif()
        set(PACKAGE_READY TRUE PARENT_SCOPE)
    endif ()
endfunction()