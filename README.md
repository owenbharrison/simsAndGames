# simsAndGames
This repository is a compilation of my most recent Physics simulations and game clones using olc::PixelGameEngine.

## Getting them on your computer
Clone the repo
```
git clone https://github.com/owenbharrison/simsAndGames.git
```

### Running the projects

#### Visual Studio
1. Open the simsAndGames.sln file.
3. Open the Solution Explorer.
4. In Visual Studio, **right click** the solution and select `Properties`.
5. Under `Configure Startup Projects`, select `Current selection` and press `OK`.
6. Select the `Release` option under the Solution Configurations dropdown.
7. Select your desired project and press `Start Without Debugging` or `Ctrl+F5` to run!

#### CMake
1. Generate build system for Visual Studio
```
cmake -B build -G "Visual Studio 18 2026"
```
2. Build individual projects
```
cmake --build build --config Release --target raycasting
```
3. Run the project
```
"build/Release/raycasting/raycasting.exe"
```

## Gallery

Most if not all projects are now hosted on my website!!

[link](https://owenbharrison.github.io/sims_and_games/)

## Future Works
- [ ] inverse kinematics
- [ ] neuroevolution of augmenting topologies
- [x] emscripten to host projects
- [ ] terraria-esque water physics
- [ ] fourier transform
- [ ] levenshtein distance
- [ ] open world navigation
- [x] terrain generation
- [x] raycasting
