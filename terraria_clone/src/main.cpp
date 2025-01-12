/*TODO:
-flowing blocks
	-add momentum to water
	-add lava [DONE]
		-very slow(viscous)
		-water+lava=obsidian+pollution
		-add pollution
		-trees?
	-add acid, fire, gunpowder?
-little alchemy?
-powdergame?
-add textures
-add grass "grown" value
-interactive environment
	-smoke adds to pressure?
	-cfd(wind)
	-creatures
		-game ai6
		-boss ai?
			-nn to learn player habits
	-non pixel elements
		-rope bridges
		-teardown esc physics
		-grapple guns?
-player
	-wheel select controls
	-movement controls
		-physics
			-aabb collisions
	-viewport scale + transform
	-health
	-breathing?
-chunking 16x16
-shader effects
	-crt
	-chromatic abberation
	-nausea
	-edge blurring?
	-sun bloom
	-clouds
-procedural generation
	-terrain
		-perlin noise
		-fractal brownian motion
		-sum of sines [DONE]
	-biomes
	-structure generation
		-wave function collapse
-FUTURE PROOFING
-NO PREMATURE OPTIMIZATION!
*/

#define OLC_PGE_APPLICATION
#include "olcPixelGameEngine.h"

#define OLC_SOUNDWAVE
#include "olcSoundWaveEngine.h"

template<typename T>
olc::v2d_generic<T> operator+(const olc::v2d_generic<T>& a, T b) { return a+olc::v2d_generic<T>(b, b); }
template<typename T>
olc::v2d_generic<T> operator-(const olc::v2d_generic<T>& a, T b) { return a+(-b); }
#pragma endregion

//remaps value in one range to the other
float remap(float x, float a, float b, float c, float d) {
	//convert to a-b to 0-1
	float t=(x-a)/(b-a);
	//convert to 0-1 to c-d
	return c+t*(d-c);
}

float random(float a=1, float b=0) {
	static const float rand_max=RAND_MAX;

	float t=rand()/rand_max;
	return a+t*(b-a);
}

#include "tile.h"
#include "mesh.h"
#include "map.h"
#include "entity.h"

struct Game : olc::PixelGameEngine {
	Map* map=nullptr;

	float grass_timer=0;

	//other liquid speeds?
	//viscosity?
	float liquid_timer=0;

	bool debug_view=false;

	olc::Sprite* background_sprite=nullptr;
	olc::Decal* background_decal=nullptr;

	olc::sound::WaveEngine sound_engine;
	olc::sound::Wave obsidian_noise;

	//encapsulate.
	Entity player;
	olc::Sprite* player_anim_sprite=nullptr;
	olc::Decal* player_anim_decal=nullptr;
	size_t player_anim_width, player_anim_height, player_anim_num;
	float player_anim_timer=0;
	size_t player_anim_frame=0;

	Game() {
		sAppName="Terraria Clone";
	}

	void placeEntity(Entity& p) {
		//random pos in world
		p.pos.x=rand()%int(map->width-p.size.x);

		//find surface
		int j;
		for(j=map->height-1; j>=0; j--) {
			if(map->tiles[map->ix(p.pos.x, j)]==Tile::Air) break;
		}

		//place 5 above surface(including player height
		p.pos.y=j-p.size.y-5;

		//clear velocity!
		p.vel={0, 0};
	}

	bool OnUserCreate() override {
		srand(time(0));

		//based on aspect ratio, setup world dimensions.
		//change this later to be like 5000x1400 with chunking.
		int width=40;
		int height=float(width)*ScreenHeight()/ScreenWidth();
		map=new Map(width, height);

		//background photo
		background_sprite=new olc::Sprite("res/background.jpg");
		background_decal=new olc::Decal(background_sprite);

		//init audio
		sound_engine.InitialiseAudio();
		sound_engine.SetOutputVolume(.5f);
		obsidian_noise.LoadAudioWaveform("res/obsidian.wav");

		//player init
		player=Entity({0, 0}, {1.5, 2.5}, "res/player.png");
		player.walk_speed=5;
		placeEntity(player);
		player_anim_sprite=new olc::Sprite("res/spritesheet.png");
		player_anim_decal=new olc::Decal(player_anim_sprite);
		player_anim_width=9, player_anim_height=8;
		player_anim_num=player_anim_width*player_anim_height/4;

		ShowWindow(GetConsoleWindow(), SW_MINIMIZE);

		return true;
	}

	bool OnUserUpdate(float dt) {
		const olc::vf2d mouse_pos=GetMousePos();
		//should always be square.
		const olc::vf2d block_size(
			float(ScreenWidth())/map->width,
			float(ScreenHeight())/map->height
		);

		//exit on esc?
		if(GetKey(olc::ESCAPE).bHeld) return false;

#pragma region USER_INPUT
		//how can i detect changes in the tileset in the map class?
		//-> solution: privatize tile array, make setter method, simple flagging
		bool to_construct=false;

		//random terrain?
		if(GetKey(olc::Key::ENTER).bPressed||GetKey(olc::Key::RETURN).bPressed) {
			map->generateTerrain(random(10000));
			placeEntity(player);
			to_construct=true;
		}

		//DEBUG
		if(GetKey(olc::Key::P).bPressed) player.pos=mouse_pos/block_size;

		//add tiles
		bool to_place=true; Tile said_tile; int rad;
		if(GetKey(olc::Key::A).bHeld) said_tile=Tile::Air, rad=4;
		else if(GetKey(olc::Key::R).bHeld) said_tile=Tile::Rock, rad=1;
		else if(GetKey(olc::Key::W).bHeld) said_tile=Tile::Water, rad=3;
		else if(GetKey(olc::Key::S).bHeld) said_tile=Tile::Sand, rad=1;
		else if(GetKey(olc::Key::L).bHeld) said_tile=Tile::Lava, rad=2;
		else if(GetKey(olc::Key::G).bHeld) said_tile=Tile::Gravel, rad=1;
		else to_place=false;
		if(to_place) {
			olc::vi2d ij=mouse_pos/block_size;

			//place a lot of blocks at once
			for(int di=-rad; di<=rad; di++) {
				int i=ij.x+di;
				if(!map->inRangeX(i)) continue;

				for(int dj=-rad; dj<=rad; dj++) {
					int j=ij.y+dj;
					if(!map->inRangeY(j)) continue;

					//inside circle?
					if(di*di+dj*dj<=rad*rad) {
						map->tiles[map->ix(i, j)]=said_tile;
						to_construct=true;
					}
				}
			}
		}
#pragma endregion

#pragma region BLOCK UPDATE
		//should i have a tickspeed variable?
		if(grass_timer<0) {
			grass_timer=.2f;

			if(map->growGrass()) to_construct=true;
		}
		grass_timer-=dt;

		if(liquid_timer<0) {
			liquid_timer=.05f;

			if(map->moveDynamicTiles()) to_construct=true;
		}
		liquid_timer-=dt;

		//this should happen instantaneously.
		if(map->combineLiquids()) {
			//i dont know if ill keep this...
			sound_engine.PlayWaveform(&obsidian_noise);
			to_construct=true;
		}

		if(to_construct) map->constructMeshes();
#pragma endregion

#pragma region PHYSICS
		//first check onground?
		if(GetKey(olc::Key::SPACE).bPressed&&player.falling<5) player.vel.y=-10;

		const auto walk_speed=5;
		if(GetKey(olc::Key::LEFT).bHeld) player.vel.x=-walk_speed;
		if(GetKey(olc::Key::RIGHT).bHeld) player.vel.x=walk_speed;

		//apply gravity
		//wind?
		player.applyForce({0, 9.8f});

		//lovely helper
		player.updateAndCollide(dt, *map);

		//change by velocity/max_velocity?
		//walking vs running...
		if(player_anim_timer<0) {
			player_anim_timer=.05f;

			player_anim_frame++;
			if(player_anim_frame==player_anim_num) player_anim_frame=0;
		}
		player_anim_timer-=dt*abs(player.vel.x)/player.walk_speed;
#pragma endregion

#pragma region RENDER
		Clear(olc::BLACK);

		//impl parallax for background
		//revisit olc::layers video
		DrawDecal({0, 0}, background_decal, {
			float(ScreenWidth())/background_decal->sprite->width,
			float(ScreenHeight())/background_decal->sprite->height
		});

		//impl pgex::transformed_view
		//impl zoom in/out feature

		//i need to start using layers.
		if(debug_view) {
			//show grid
			for(int i=1; i<map->width; i++) {
				float x=block_size.x*i;
				DrawLineDecal({x, 0.f}, {x, float(ScreenHeight())}, olc::GREY);
			}
			for(int j=1; j<map->height; j++) {
				float y=block_size.y*j;
				DrawLineDecal({0.f, y}, {float(ScreenWidth()), y}, olc::GREY);
			}
		}

		//draw meshes
		for(const auto& m:map->meshes) {
			//pick color
			//impl sample texure atlas
			olc::Pixel col;
			switch(m.tile) {
				case Tile::Grass: col=olc::GREEN; break;
				case Tile::Dirt: col=olc::Pixel(160, 82, 45); break;
				case Tile::Rock: col=olc::Pixel(150, 150, 150); break;
				case Tile::Water: col=olc::Pixel(0, 100, 255); break;
				case Tile::Lava: col=olc::Pixel(240, 90, 0); break;
				case Tile::Obsidian: col=olc::Pixel(40, 0, 80); break;
				case Tile::Sand: col=olc::Pixel(230, 210, 100); break;
				case Tile::Gravel: col=olc::Pixel(138, 123, 116); break;
			}

			//calculate mesh pos, size
			//impl texture repetition
			FillRectDecal(block_size*m.ij, block_size*m.wh, col);
		}

		if(GetKey(olc::Key::D).bPressed) debug_view^=true;

		if(debug_view) {
			//show outlines
			for(const auto& m:map->meshes) {
				olc::vf2d tl=block_size*m.ij, br=tl+block_size*m.wh;
				olc::vf2d tr(br.x, tl.y), bl(tl.x, br.y);

				olc::Pixel col;
				switch((m.wh.x==1)+(m.wh.y==1)) {
					case 0: col=olc::BLUE; break;
					case 1: col=olc::YELLOW; break;
					case 2: col=olc::RED; break;
				}

				//two triangles
				DrawLineDecal(tl, tr, col);
				DrawLineDecal(tr, br, col);
				DrawLineDecal(br, bl, col);
				DrawLineDecal(bl, tl, col);
				DrawLineDecal(tl, br, col);
			}

			//mouse thing
			olc::vi2d ij=mouse_pos/block_size;
			olc::vf2d floor=block_size*ij;

			//coordinates
			auto str=std::to_string(ij.x)+','+std::to_string(ij.y);
			DrawStringDecal(floor, str);

			//crosshair?
			DrawLineDecal(floor, {floor.x+block_size.x, floor.y}, olc::RED);
			DrawLineDecal(floor, {floor.x, floor.y+block_size.y}, olc::BLUE);

			//more debug
			int num_water=0, num_lava=0, num_sand=0;
			for(int i=0; i<map->width*map->height; i++) {
				switch(map->tiles[i]) {
					case Tile::Water: num_water++; break;
					case Tile::Lava: num_lava++; break;
					case Tile::Sand: num_sand++; break;
				}
			}
			DrawStringDecal({0, 0}, "Water: "+std::to_string(num_water));
			DrawStringDecal({0, 8}, "Lava: "+std::to_string(num_lava));
			DrawStringDecal({0, 16}, "Sand: "+std::to_string(num_sand));
		}

		//draw player
		//should i add entity::draw(olc::pixelGameEngine&) ??
		{
			//tidy up
			olc::vf2d corner=block_size*player.pos, size=block_size*player.size;

			//avoid modulus?
			int j=player_anim_frame/player_anim_width;
			olc::vi2d ij(player_anim_frame-player_anim_width*j, j);

			olc::vf2d decal_size(
				float(player_anim_sprite->width)/player_anim_width,
				float(player_anim_sprite->height)/player_anim_height
			);
			//walk direction
			if(player.vel.x<0) DrawPartialDecal(corner, size, player_anim_decal, decal_size*ij, decal_size);
			else DrawPartialDecal({corner.x+size.x, corner.y}, {-size.x, size.y}, player_anim_decal, decal_size*ij, decal_size);

			if(debug_view) {
				olc::vf2d br=corner+size;
				olc::vf2d tr(br.x, corner.y), bl(corner.x, br.y);

				//two triangles
				DrawLineDecal(corner, tr, olc::BLACK);
				DrawLineDecal(tr, br, olc::BLACK);
				DrawLineDecal(br, bl, olc::BLACK);
				DrawLineDecal(bl, corner, olc::BLACK);
				DrawLineDecal(corner, br, olc::BLACK);

				DrawStringDecal(corner, "fall: "+std::to_string(player.falling));
			}
		}
#pragma endregion

		return true;
	}
};

int main() {
	Game g;
	int width=1000;
	if(g.Construct(width, width*9/16, 1, 1, false, true, false, true)) g.Start();

	return 0;
}