@echo off
sokol-shdc --input src/shd.glsl --output src/shd.h --slang glsl430:hlsl5:metal_macos
pause