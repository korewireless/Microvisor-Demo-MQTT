cmake_minimum_required(VERSION 3.14)

# Set application data
set(APP "Microvisor MQTT Demo")
set(VERSION_NUMBER "1.0.0")
set(BUILD_NUMBER "1")

message("Building ${APP} ${VERSION_NUMBER} build ${BUILD_NUMBER}")

# Make app data available to the compiler
add_compile_definitions(APP_NAME="${APP}")
add_compile_definitions(APP_VERSION="${VERSION_NUMBER}")
add_compile_definitions(BUILD_NUM=${BUILD_NUMBER})

# Compile app source code file(s)
add_executable(${PROJECT_NAME}
    main.c
    config_handler.c
    log_helper.c
    mqtt_handler.c
    network_helper.c
    stm32u5xx_hal_timebase_tim_template.c
    uart_logging.c
    work.c
)

# Link built libraries
target_link_libraries(${PROJECT_NAME} LINK_PUBLIC
    ST_Code
    twilio-microvisor-hal-stm32u5
    FreeRTOS)

# Build .bin file required for app bundle creation
add_custom_command(OUTPUT BUILD_ARTIFACTS DEPENDS ${PROJECT_NAME}
    COMMAND mv "${PROJECT_NAME}" "${PROJECT_NAME}.elf"
    COMMAND ${CMAKE_SIZE} --format=berkeley "${PROJECT_NAME}.elf"
    COMMAND ${CMAKE_OBJDUMP} -h -S "${PROJECT_NAME}.elf" > "${PROJECT_NAME}.list"
    COMMAND ${CMAKE_OBJCOPY} --output-target ihex "${PROJECT_NAME}.elf" "${PROJECT_NAME}.hex"
    COMMAND ${CMAKE_OBJCOPY} --input-target ihex --output-target binary --gap-fill 0xFF "${PROJECT_NAME}.hex" "${PROJECT_NAME}.bin"
)

# Add target for the artifacts
add_custom_target(build_artifacts ALL DEPENDS BUILD_ARTIFACTS)
