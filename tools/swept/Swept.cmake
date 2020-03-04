set(swept_SCRIPTS
        swept/main.lua
        swept/init.lua
        swept/sys/utils.lua
        swept/sys/sh.lua
        swept/sys/argparse.lua
        swept/sys/json.lua
        swept/sys/http.lua
        swept/sys/logger.lua
        swept/sys/scripter.lua
        swept/sys/reporter.lua
        swept/sys/testcase.lua
        swept/sys/sweeper.lua)

set(swept_SCRIPTS_DEFS  ${CMAKE_CURRENT_SOURCE_DIR}/swept/_defs.h)
set(swept_SCRIPTS_DECLS ${CMAKE_CURRENT_SOURCE_DIR}/swept/_decls.h)

file(WRITE ${swept_SCRIPTS_DECLS} "extern \"C\" {\n")
file(WRITE ${swept_SCRIPTS_DEFS} "\n")

foreach(SCRIPT ${swept_SCRIPTS})
    string(REPLACE "/" "_" tmp_SCRIPT_NAME ${SCRIPT})
    get_filename_component(SCRIPT_NAME ${tmp_SCRIPT_NAME} NAME_WE)

    # extern script parameters into a header file that can be included when
    # building scripting code in C/C++
    file(APPEND ${swept_SCRIPTS_DECLS} "extern const char ${SCRIPT_NAME}[];\n")
    file(APPEND ${swept_SCRIPTS_DECLS} "extern size_t ${SCRIPT_NAME}_size;\n")

    # declare a script entry in a header file that can be used when building scripts list
    file(APPEND ${swept_SCRIPTS_DEFS} "{\"${SCRIPT_NAME}\", ${SCRIPT_NAME}_size, ${SCRIPT_NAME} },\n")

    # compile the script and generate C code
    suil_lua2c(${SCRIPT_NAME}
            TARGET swept
            SCRIPT ${CMAKE_CURRENT_SOURCE_DIR}/${SCRIPT})
endforeach()

# terminate header file
file(APPEND ${swept_SCRIPTS_DECLS} "};\n")
file(APPEND ${swept_SCRIPTS_DEFS} "{nullptr, 0, nullptr },\n")