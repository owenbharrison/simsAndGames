cmake_minimum_required(VERSION 3.15)
project(simsAndGames LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

#common include dir
include_directories(${CMAKE_SOURCE_DIR}/common)

#list of all projects
set(PROJECTS
    3d_fx
    3d_physics
    a_star_nav
    block_craft
    clock
    cloth
    eulerian_fluid
    flip_fluid
    fluid
    fracture
    fruit_ninja
    inverse_kinematics
    jelly_car_clone
    marching_cubes
    minesweeper
    particles
    pixels
    procedural_audio
    raycasting
    rigging
    rigid_bodies
    shaders
    slicer
    smoke
    sudoku
    targetting
    terraria_clone
    testing
    tracking
    voxels
    wordle
)

foreach(proj ${PROJECTS})
    add_executable(${proj} ${proj}/src/main.cpp)

    #output: build/<config>/<project>
    set_target_properties(${proj} PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY_DEBUG ${CMAKE_BINARY_DIR}/Debug/${proj}
        RUNTIME_OUTPUT_DIRECTORY_RELEASE ${CMAKE_BINARY_DIR}/Release/${proj}
        RUNTIME_OUTPUT_DIRECTORY_RELWITHDEBINFO ${CMAKE_BINARY_DIR}/RelWithDebInfo/${proj}
        RUNTIME_OUTPUT_DIRECTORY_MINSIZEREL ${CMAKE_BINARY_DIR}/MinSizeRel/${proj}
    )

    #does project have assets folder?
    if(EXISTS ${CMAKE_SOURCE_DIR}/${proj}/assets)
        #copy assets before build
        add_custom_command(TARGET ${proj} PRE_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_directory
                ${CMAKE_SOURCE_DIR}/${proj}/assets
                $<TARGET_FILE_DIR:${proj}>/assets
        )
    endif()
endforeach()