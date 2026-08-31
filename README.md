# simsAndGames
This repository is a compilation of my most recent c/c++ physics simulations and game prototypes!\
Most projects use either
[sokol](https://github.com/floooh/sokol)
or
[olcPixelGameEngine](https://github.com/onelonecoder/olcpixelgameengine)
for rendering and input.

### Getting them on your computer
Clone the repo
```
git clone https://github.com/owenbharrison/simsAndGames.git
```

### Running the projects with CMake

1. Configure and generate build files into a folder named build
```
cmake -S . -B build
```
2. Compile all projects
```
cmake --build build
```
3. Navigate to executable and run project!
```
cd build/raycasting/Debug
./raycasting
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
