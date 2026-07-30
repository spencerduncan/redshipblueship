message(STATUS "Copying otr files...")

# `cmake -E copy <file> <dir>/` fails with "Invalid argument" when <dir> does not
# exist, and nothing else creates ${BINARY_DIR}/soh — so every copy below into it
# printed an error and silently did nothing on a fresh tree (the MM side already
# does this, via `-E make_directory ${CMAKE_BINARY_DIR}/mm` in its own targets).
# Noticed while fixing #560's archive staging; the install rules read this
# directory, so the failure was not harmless.
execute_process(COMMAND ${CMAKE_COMMAND} -E make_directory ${BINARY_DIR}/soh)

if(NOT ONLYSOHOTR AND EXISTS ${SOURCE_DIR}/games/oot/oot.o2r)
    execute_process(COMMAND ${CMAKE_COMMAND} -E copy oot.o2r ${SOURCE_DIR})
    execute_process(COMMAND ${CMAKE_COMMAND} -E copy oot.o2r ${BINARY_DIR}/soh/)
    message(STATUS "Copied oot.o2r")
endif()
if(NOT ONLYSOHOTR AND EXISTS ${SOURCE_DIR}/games/oot/oot-mq.o2r)
    execute_process(COMMAND ${CMAKE_COMMAND} -E copy oot-mq.o2r ${SOURCE_DIR})
    execute_process(COMMAND ${CMAKE_COMMAND} -E copy oot-mq.o2r ${BINARY_DIR}/soh/)
    message(STATUS "Copied oot-mq.o2r")
endif()
if(EXISTS ${SOURCE_DIR}/games/oot/soh.o2r)
    execute_process(COMMAND ${CMAKE_COMMAND} -E copy soh.o2r ${SOURCE_DIR})
    execute_process(COMMAND ${CMAKE_COMMAND} -E copy soh.o2r ${BINARY_DIR}/soh/)
    # And the build root (#560). ${BINARY_DIR}/soh above is where only the
    # install rules look; the CTest rows run with cwd = ${BINARY_DIR} and the
    # binary they run lives there too, so Ship::Context::LocateFileAcrossAppDirs
    # (app-config dir -> exe dir -> "./<name>") only ever finds a port archive
    # sitting directly in ${BINARY_DIR}. Without this copy every row in the
    # redship and rando tiers boots with ZERO archives mounted, which pauses
    # libultraship's resource thread pool for the life of the process
    # (ResourceManager.cpp: "Nothing ever unpauses the thread pool") and
    # deadlocks any synchronous load. CI stages the same file the same way in
    # its own workflows, because there it is DOWNLOADED as an artifact rather
    # than produced by this script.
    execute_process(COMMAND ${CMAKE_COMMAND} -E copy soh.o2r ${BINARY_DIR}/)
    message(STATUS "Copied soh.o2r")
endif()

# Additionally for Windows, copy the otrs to the target dir, side by side with soh.exe
if(SYSTEM_NAME MATCHES "Windows")
    if(NOT ONLYSOHOTR AND EXISTS ${SOURCE_DIR}/games/oot/oot.o2r)
        execute_process(COMMAND ${CMAKE_COMMAND} -E copy oot.o2r ${TARGET_DIR})
    endif()
    if(NOT ONLYSOHOTR AND EXISTS ${SOURCE_DIR}/games/oot/oot-mq.o2r)
        execute_process(COMMAND ${CMAKE_COMMAND} -E copy oot-mq.o2r ${TARGET_DIR})
    endif()
    if(EXISTS ${SOURCE_DIR}/games/oot/soh.o2r)
        execute_process(COMMAND ${CMAKE_COMMAND} -E copy soh.o2r ${TARGET_DIR})
    endif()
endif()

if(NOT ONLYSOHOTR AND (NOT EXISTS ${SOURCE_DIR}/oot.o2r AND NOT EXISTS ${SOURCE_DIR}/oot-mq.o2r))
    message(FATAL_ERROR "Failed to copy. No OTR files found.")
endif()
if(NOT EXISTS ${SOURCE_DIR}/soh.o2r)
    message(FATAL_ERROR "Failed to copy. No soh OTR found.")
endif()
