# Main CMakeLists for the arduino modules


add_subdirectory(mag-deck)
add_subdirectory(temp-deck)
add_subdirectory(thermo-cycler)

add_custom_target(zip-all)
add_dependencies(zip-all magdeck-zip tempdeck-zip thermocycler-zip)
