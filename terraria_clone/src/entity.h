#pragma once
#ifndef ENTITY_CLASS_H
#define ENTITY_CLASS_H

//rule of three?
struct Entity {
	olc::vf2d pos, vel, acc;
	olc::vf2d size;
	float walk_speed=3;

	int falling=0;

	olc::Sprite* sprite=nullptr;
	olc::Decal* decal=nullptr;

	Entity() {}

	//how else can i pass in a sprite...
	Entity(olc::vf2d p, olc::vf2d s, std::string str) : pos(p), size(s) {
		sprite=new olc::Sprite(str);
		decal=new olc::Decal(sprite);
	}

	~Entity() {
		delete sprite;
		delete decal;
	}

	void applyForce(olc::vf2d f) {
		acc+=f;
	}

	//do i need a whole map reference here?
	void updateAndCollide(float dt, const Map& m) {
		//updating
		vel+=acc*dt;

		//should this be here?
		vel.x*=1-4.5f*dt;

		falling++;

		//reset forces
		acc*=0;

		//collisions
		//world bounding box collisions?
		//or: "entity fell out of the world!"
		pos.x+=dt*vel.x;
		for(int j=pos.y; j<=pos.y+size.y-.1f; j++) {
			if(!m.inRangeY(j)) continue;

			if(vel.x<0) {//check left
				int i=pos.x;
				if(m.inRangeX(i)&&getType(m.tiles[m.ix(i, j)])==TileType::Solid) {
					pos.x=1+i;
					vel.x=0;
					break;
				}
			} else {//check right
				int i=pos.x+size.x;
				if(m.inRangeX(i)&&getType(m.tiles[m.ix(i, j)])==TileType::Solid) {
					pos.x=i-size.x;
					vel.x=0;
					break;
				}
			}
		}

		pos.y+=dt*vel.y;
		for(int i=pos.x; i<=pos.x+size.x-.1f; i++) {
			if(!m.inRangeX(i)) continue;

			if(vel.y<0) {//check above
				int j=pos.y;
				if(m.inRangeY(j)&&getType(m.tiles[m.ix(i, j)])==TileType::Solid) {
					pos.y=1+j;
					vel.y=0;
					break;
				}
			} else {//check below
				int j=pos.y+size.y;
				if(m.inRangeY(j)&&getType(m.tiles[m.ix(i, j)])==TileType::Solid) {
					pos.y=j-size.y;
					falling=0;
					vel.y=0;
					break;
				}
			}
		}
	}
};
#endif//ENTITY_CLASS_H