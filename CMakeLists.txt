cmake_minimum_required(VERSION 3.25)

project(sims_and_games)

#olc stuff??
find_package(OpenGL QUIET)
find_package(X11 QUIET)
find_package(PNG QUIET)
find_package(Threads QUIET)

function(add_project name)
    add_executable(${name}
            src/main.cpp
    )

    target_compile_features(${name} PRIVATE cxx_std_17)

    target_include_directories(${name} PRIVATE
            ${CMAKE_SOURCE_DIR}
    )

    #olc stuff??
    if(OpenGL_FOUND)
        target_link_libraries(${name} PRIVATE OpenGL::GL)
    endif()
    if(X11_FOUND)
        target_link_libraries(${name} PRIVATE X11::X11)
    endif()
    if(PNG_FOUND)
        target_link_libraries(${name} PRIVATE PNG::PNG)
    endif()
    if(Threads_FOUND)
        target_link_libraries(${name} PRIVATE Threads::Threads)
    endif()
    if(WIN32)
        target_link_libraries(${name} PRIVATE gdiplus dwmapi shlwapi)
    endif()

    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/assets")
        add_custom_command(TARGET ${name} POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_directory
                "${CMAKE_CURRENT_SOURCE_DIR}/assets"
                "$<TARGET_FILE_DIR:${name}>/assets"
        )
    endif()
endfunction()

add_subdirectory(3d_physics)
add_subdirectory(a_star_nav)#olc
add_subdirectory(asteroids)
add_subdirectory(audio_viz)
add_subdirectory(boids)
add_subdirectory(clock_sim)
add_subdirectory(cloth)
add_subdirectory(eulerian_fluid)#olc
add_subdirectory(fracture)
add_subdirectory(fruit_ninja)#olc
add_subdirectory(geometrize)#olc
add_subdirectory(jelly_car_clone)#olc
add_subdirectory(magnets)
add_subdirectory(minesweeper3d)
add_subdirectory(particles)
add_subdirectory(phys3d)
add_subdirectory(pixels)#olc
add_subdirectory(planet_game)
add_subdirectory(raycasting)
add_subdirectory(rigid_bodies)#olc
add_subdirectory(robot)
add_subdirectory(rubiks_cube)
add_subdirectory(sketcher)
add_subdirectory(slicer)
add_subdirectory(smoke)
add_subdirectory(sokol_testing)
add_subdirectory(spiders)
add_subdirectory(splitter)
add_subdirectory(steering_behaviors)
add_subdirectory(targeting)#olc
add_subdirectory(terrain)
add_subdirectory(terraria_clone)
add_subdirectory(testing)
add_subdirectory(tracking)#olc