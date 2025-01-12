#pragma once
#ifndef MESH_STRUCT_H

//is mesh the right name?
//how can i get this to store a texture and a texturerect
//how can i get the texture sampling to repeat based on mesh::wh

struct Mesh {
	olc::vi2d ij, wh;
	Tile tile;//should store ptr?
};
#endif//MESH_STRUCT_H