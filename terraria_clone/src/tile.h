#pragma once
#ifndef TILE_STRUCT_H
#define TILE_STRUCT_H

//impl sand+lava=glass OR ash

//tile::getmeshable?

//how should i store tiles??
//its going to be a ton of copies of the same type...
//use ids with texture atlas...
//should also store block "states" for grown/ungrown dirt/mud, liquid level
enum struct Tile {
	Air=0,
	Grass,//should be a "variant" of dirt.
	Dirt,
	Rock,
	Water,
	Lava,
	Obsidian,
	Sand,
	Gravel,
	NUM_TILES//do we need this
};

enum struct TileProps {
	None=0b000,
	MoveDown=0b001,
	MoveDiag=0b010,
	MoveSide=0b100
};

//they really are just integers...
TileProps operator|(const TileProps& a, const TileProps& b) {
	return TileProps(int(a)|int(b));
}
bool operator&(const TileProps& a, const TileProps& b) {
	return int(a)&int(b);
}

//this way we dont have to store tileprops in every tile...
//there might be better ways, who cares.
TileProps getProperties(const Tile& t) {
	switch(t) {
		case Tile::Water: case Tile::Lava:
			return TileProps::MoveDown|TileProps::MoveDiag|TileProps::MoveSide;
		case Tile::Sand:
			return TileProps::MoveDown|TileProps::MoveDiag;
		case Tile::Gravel:
			return TileProps::MoveDown;
		default:
			return TileProps::None;
	}
}

enum struct TileType {
	Gas,
	Liquid,
	Solid
};

//for density
int operator-(const TileType& a, const TileType& b) {
	return int(a)-int(b);
}

//how can i store all the states and properties in the 
TileType getType(const Tile& t) {
	switch(t) {
		case Tile::Air:
			return TileType::Gas;
		case Tile::Water: case Tile::Lava:
			return TileType::Liquid;
		default:
			return TileType::Solid;
	}
}
#endif//TILE_STRUCT_H