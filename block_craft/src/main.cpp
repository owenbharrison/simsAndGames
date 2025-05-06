#include "common/3d/engine_3d.h"
using olc::vf2d;

#include "chunk.h"

#include "common/stopwatch.h"
#include "common/utils.h"

int neg_mod(int a, int b) {
	return (b+a%b)%b;
}

struct BlockGame : Engine3D {
	BlockGame() {
		sAppName="Block Game";
	}

	float cam_yaw=0;
	float cam_pitch=0;

	//debug toggles
	bool show_outlines=false;
	bool show_render=false;
	bool show_grid=false;
	bool show_normals=false;

	//world stuff
	std::list<Chunk> chunks;
	std::unordered_map<ChunkCoord, Chunk*> chunk_map;

	//player stuff
	bool player_flying=true;
	vf3d player_pos;
	vf3d player_vel;
	vf3d player_acc;
	const vf3d player_size{.9f, 1.8f, .9f};
	const vf3d player_head_factor{.5f, .9f, .5f};
	float player_airtimer=0;

	//diagnostic info
	bool to_time=false;
	cmn::Stopwatch update_timer, geom_timer, pc_timer, render_timer;

	bool user_create() override {
		srand(time(0));

		std::cout<<"Press ESC for integrated console.\n"
			"  then type help for help.\n";
		ConsoleCaptureStdOut(true);

#pragma region CHUNK GENERATION
		//load chunks
		const int num_x=1;
		const int num_y=1;
		const int num_z=1;
		Chunk** grid=nullptr;
		grid=new Chunk*[num_x*num_y*num_z];
		auto ix=[&] (int i, int j, int k) {
			return i+num_x*(j+num_y*k);
		};

		//allocate grid
		for(int i=0; i<num_x; i++) {
			for(int j=0; j<num_y; j++) {
				for(int k=0; k<num_z; k++) {
					chunks.push_back(Chunk(i, j, k));
					auto c=&chunks.back();
					grid[ix(i, j, k)]=c;
					chunk_map[c->coord]=c;
				}
			}
		}

		//set neighbors
		for(int i=0; i<num_x; i++) {
			for(int j=0; j<num_y; j++) {
				for(int k=0; k<num_z; k++) {
					Chunk* curr=grid[ix(i, j, k)];
					if(i>0) curr->left=grid[ix(i-1, j, k)];
					if(i<num_x-1) curr->right=grid[ix(i+1, j, k)];
					if(j>0) curr->bottom=grid[ix(i, j-1, k)];
					if(j<num_y-1) curr->top=grid[ix(i, j+1, k)];
					if(k>0) curr->back=grid[ix(i, j, k-1)];
					if(k<num_z-1) curr->front=grid[ix(i, j, k+1)];
				}
			}
		}

		//fill each chunk with an ellipsoid
		float a=.5f*Chunk::width;
		float b=.5f*Chunk::height;
		float c=.5f*Chunk::depth;
		for(auto& ch:chunks) {
			for(int i=0; i<Chunk::width; i++) {
				float x=i-a;
				for(int j=0; j<Chunk::height; j++) {
					float y=j-b;
					for(int k=0; k<Chunk::depth; k++) {
						float z=k-c;
						float dist=x*x/a/a+y*y/b/b+z*z/c/c;
						ch.blocks[Chunk::ix(i, j, k)]=dist<1;
					}
				}
			}
		}

		delete[] grid;

		for(auto& c:chunks) {
			c.triangulate();
		}
#pragma endregion

		return true;
	}

	bool OnConsoleCommand(const std::string& line) override {
		std::stringstream line_str(line);
		std::string cmd; line_str>>cmd;

		if(cmd=="clear") {
			ConsoleClear();

			return true;
		}

		if(cmd=="time") {
			to_time=true;

			return true;
		}

		if(cmd=="keybinds") {
			std::cout<<
				"  ARROWS   look up, down, left, right\n"
				"  WASD     move forward, back, left, right\n"
				"  SPACE    move up/jump\n"
				"  SHIFT    move down \n"
				"  L        set light pos\n"
				"  C        toggle player collisons\n"
				"  O        toggle outlines\n"
				"  G        toggle grid\n"
				"  N        toggle normals\n"
				"  R        toggle render view\n"
				"  ESC      toggle integrated console\n";

			return true;
		}

		if(cmd=="help") {
			std::cout<<
				"  clear        clears the console\n"
				"  time         get diagnostics for each stage of game loop\n"
				"  keybinds     which keys to press for this program?\n";

			return true;
		}

		std::cout<<"unknown command. type help for list of commands.\n";

		return false;
	}

	void handleUserInput(float dt) {
		if(GetKey(olc::Key::C).bPressed) {
			if(player_flying) {
				//account for camera disparity
				player_pos=cam_pos-player_head_factor*player_size;
				player_vel*=0;
			}
			player_flying^=true;
		}

		//look up, down
		if(GetKey(olc::Key::UP).bHeld) cam_pitch+=dt;
		if(cam_pitch>cmn::Pi/2) cam_pitch=cmn::Pi/2-.001f;
		if(GetKey(olc::Key::DOWN).bHeld) cam_pitch-=dt;
		if(cam_pitch<-cmn::Pi/2) cam_pitch=.001f-cmn::Pi/2;

		//look left, right
		if(GetKey(olc::Key::LEFT).bHeld) cam_yaw-=dt;
		if(GetKey(olc::Key::RIGHT).bHeld) cam_yaw+=dt;

		//polar to cartesian
		cam_dir=vf3d(
			std::cosf(cam_yaw)*std::cosf(cam_pitch),
			std::sinf(cam_pitch),
			std::sinf(cam_yaw)*std::cosf(cam_pitch)
		);

		if(!player_flying) {
			//walk forwards, backwards
			vf2d movement;
			vf2d fb_dir(std::cosf(cam_yaw), std::sinf(cam_yaw));
			if(GetKey(olc::Key::W).bHeld) movement+=5.f*fb_dir;
			else if(GetKey(olc::Key::S).bHeld) movement-=3.f*fb_dir;

			//walk left, right
			vf2d lr_dir(fb_dir.y, -fb_dir.x);
			if(GetKey(olc::Key::A).bHeld) movement+=4.f*lr_dir;
			if(GetKey(olc::Key::D).bHeld) movement-=4.f*lr_dir;

			player_vel.x=movement.x;
			player_vel.z=movement.y;

			//jumping
			if(GetKey(olc::Key::SPACE).bPressed&&player_airtimer<.05f) {
				player_vel.y=4;
			}

			//account for camera disparity
			cam_pos=player_pos+player_head_factor*player_size;
		} else {
			//move up, down
			if(GetKey(olc::Key::SPACE).bHeld) cam_pos.y+=4.f*dt;
			if(GetKey(olc::Key::SHIFT).bHeld) cam_pos.y-=4.f*dt;

			//move forward, backward
			vf3d fb_dir(std::cosf(cam_yaw), 0, std::sinf(cam_yaw));
			if(GetKey(olc::Key::W).bHeld) cam_pos+=5.f*dt*fb_dir;
			if(GetKey(olc::Key::S).bHeld) cam_pos-=3.f*dt*fb_dir;

			//move left, right
			vf3d lr_dir(fb_dir.z, 0, -fb_dir.x);
			if(GetKey(olc::Key::A).bHeld) cam_pos+=4.f*dt*lr_dir;
			if(GetKey(olc::Key::D).bHeld) cam_pos-=4.f*dt*lr_dir;
		}

		//set light to camera
		if(GetKey(olc::Key::L).bHeld) light_pos=cam_pos;

		//ui toggles
		if(GetKey(olc::Key::O).bPressed) show_outlines^=true;
		if(GetKey(olc::Key::G).bPressed) show_grid^=true;
		if(GetKey(olc::Key::N).bPressed) show_normals^=true;
		if(GetKey(olc::Key::R).bPressed) show_render^=true;
	}

	bool user_update(float dt) override {
		if(to_time) update_timer.start();

		//open and close the integrated console
		if(GetKey(olc::Key::ESCAPE).bPressed) ConsoleShow(olc::Key::ESCAPE);

		//only allow input when console NOT open
		if(!IsConsoleShowing()) handleUserInput(dt);

		if(!player_flying) {
			//gravity
			player_acc.y-=3.2f;

			//kinematics
			player_vel+=player_acc*dt;

			//reset forces
			player_acc*=0;

			//increment airtimer
			player_airtimer+=dt;

			//how in the world to shorten these functions???
			auto collideInX=[&] () {
				for(int wj=player_pos.y; wj<=player_pos.y+player_size.y-.1f; wj++) {
					int cj=wj/Chunk::height;
					int j=neg_mod(wj, Chunk::height);
					for(int wk=player_pos.z; wk<=player_pos.z+player_size.z-.1f; wk++) {
						int ck=wk/Chunk::depth;
						int k=neg_mod(wk, Chunk::depth);
						if(player_vel.x<0) {
							int wi=player_pos.x;
							int ci=wi/Chunk::width;
							auto it=chunk_map.find({ci, cj, ck});
							if(it!=chunk_map.end()) {
								Chunk* c=it->second;
								int i=neg_mod(wi, Chunk::width);
								if(c->blocks[Chunk::ix(i, j, k)]) {
									player_pos.x=1+wi;
									player_vel.x=0;
									break;
								}
							}
						} else {
							int wi=player_pos.x+player_size.x;
							int ci=wi/Chunk::width;
							auto it=chunk_map.find({ci, cj, ck});
							if(it!=chunk_map.end()) {
								Chunk* c=it->second;
								int i=neg_mod(wi, Chunk::width);
								if(c->blocks[Chunk::ix(i, j, k)]) {
									player_pos.x=wi-player_size.x;
									player_vel.x=0;
									break;
								}
							}
						}
					}
				}
			};
			auto collideInZ=[&] () {
				for(int wj=player_pos.y; wj<=player_pos.y+player_size.y-.1f; wj++) {
					int cj=wj/Chunk::height;
					int j=neg_mod(wj, Chunk::height);
					for(int wi=player_pos.x; wi<=player_pos.x+player_size.x-.1f; wi++) {
						int ci=wi/Chunk::width;
						int i=neg_mod(wi, Chunk::width);
						if(player_vel.z<0) {
							int wk=player_pos.z;
							int ck=wk/Chunk::depth;
							auto it=chunk_map.find({ci, cj, ck});
							if(it!=chunk_map.end()) {
								Chunk* c=it->second;
								int k=neg_mod(wk, Chunk::depth);
								if(c->blocks[Chunk::ix(i, j, k)]) {
									player_pos.z=1+wk;
									player_vel.z=0;
									break;
								}
							}
						} else {
							int wk=player_pos.z+player_size.z;
							int ck=wk/Chunk::depth;
							auto it=chunk_map.find({ci, cj, ck});
							if(it!=chunk_map.end()) {
								Chunk* c=it->second;
								int k=neg_mod(wk, Chunk::depth);
								if(c->blocks[Chunk::ix(i, j, k)]) {
									player_pos.z=wk-player_size.z;
									player_vel.z=0;
									break;
								}
							}
						}
					}
				}
			};

			//prioritize speedy zx direction
			bool x_first=std::fabsf(player_vel.x)>std::fabsf(player_vel.z);
			if(x_first) {
				player_pos.x+=player_vel.x*dt;
				collideInX();
			}
			player_pos.z+=player_vel.z*dt;
			collideInZ();
			if(!x_first) {
				player_pos.x+=player_vel.x*dt;
				collideInX();
			}

			//always do y after?
			player_pos.y+=player_vel.y*dt;
			for(int wi=player_pos.x; wi<=player_pos.x+player_size.x-.1f; wi++) {
				int ci=wi/Chunk::width;
				int i=neg_mod(wi, Chunk::width);
				for(int wk=player_pos.z; wk<=player_pos.z+player_size.z-.1f; wk++) {
					int ck=wk/Chunk::depth;
					int k=neg_mod(wk, Chunk::depth);
					if(player_vel.y<0) {
						int wj=player_pos.y;
						int cj=wj/Chunk::height;
						auto it=chunk_map.find({ci, cj, ck});
						if(it!=chunk_map.end()) {
							Chunk* c=it->second;
							int j=neg_mod(wj, Chunk::height);
							if(c->blocks[Chunk::ix(i, j, k)]) {
								player_pos.y=1+wj;
								player_vel.y=0;
								player_airtimer=0;
								break;
							}
						}
					} else {
						int wj=player_pos.y+player_size.y;
						int cj=wj/Chunk::height;
						auto it=chunk_map.find({ci, cj, ck});
						if(it!=chunk_map.end()) {
							Chunk* c=it->second;
							int j=neg_mod(wj, Chunk::height);
							if(c->blocks[Chunk::ix(i, j, k)]) {
								player_pos.y=wj-player_size.y;
								player_vel.y=0;
								break;
							}
						}
					}
				}
			}
		}

		if(to_time) {
			update_timer.stop();
			auto dur=update_timer.getMicros();
			std::cout<<"  update: "<<dur<<"us ("<<(dur/1000.f)<<"ms)\n";
		}

		return true;
	}

#pragma region GEOMETRY HELPERS
	void addAABB(const AABB3& box, const olc::Pixel& col) {
		//corner vertexes
		vf3d v0=box.min, v7=box.max;
		vf3d v1(v7.x, v0.y, v0.z);
		vf3d v2(v0.x, v7.y, v0.z);
		vf3d v3(v7.x, v7.y, v0.z);
		vf3d v4(v0.x, v0.y, v7.z);
		vf3d v5(v7.x, v0.y, v7.z);
		vf3d v6(v0.x, v7.y, v7.z);
		//bottom
		Line l1{v0, v1}; l1.col=col;
		lines_to_project.push_back(l1);
		Line l2{v1, v3}; l2.col=col;
		lines_to_project.push_back(l2);
		Line l3{v3, v2}; l3.col=col;
		lines_to_project.push_back(l3);
		Line l4{v2, v0}; l4.col=col;
		lines_to_project.push_back(l4);
		//sides
		Line l5{v0, v4}; l5.col=col;
		lines_to_project.push_back(l5);
		Line l6{v1, v5}; l6.col=col;
		lines_to_project.push_back(l6);
		Line l7{v2, v6}; l7.col=col;
		lines_to_project.push_back(l7);
		Line l8{v3, v7}; l8.col=col;
		lines_to_project.push_back(l8);
		//top
		Line l9{v4, v5}; l9.col=col;
		lines_to_project.push_back(l9);
		Line l10{v5, v7}; l10.col=col;
		lines_to_project.push_back(l10);
		Line l11{v7, v6}; l11.col=col;
		lines_to_project.push_back(l11);
		Line l12{v6, v4}; l12.col=col;
		lines_to_project.push_back(l12);
	}
#pragma endregion

	bool user_geometry() override {
		if(to_time) geom_timer.start();
		
		//add all chunk meshes
		for(const auto& c:chunks) {
			tris_to_project.insert(tris_to_project.end(), c.triangles.begin(), c.triangles.end());
		}

		//add player aabb
		if(player_flying) addAABB({player_pos, player_pos+player_size}, olc::GREEN);

		//grid lines on xy, yz, zx planes
		if(show_normals) {
			const float sz=.2f;
			for(const auto& t:tris_to_project) {
				vf3d ctr=t.getCtr();
				Line l{ctr, ctr+sz*t.getNorm()};
				l.col=t.col;
				lines_to_project.push_back(l);
			}
		}

		//add all triangle outlines
		if(show_outlines) {
			for(const auto& t:tris_to_project) {
				Line l1{t.p[0], t.p[1]}; l1.col=olc::BLACK;
				lines_to_project.push_back(l1);
				Line l2{t.p[1], t.p[2]}; l2.col=olc::BLACK;
				lines_to_project.push_back(l2);
				Line l3{t.p[2], t.p[0]}; l3.col=olc::BLACK;
				lines_to_project.push_back(l3);
			}
		}

		if(to_time) {
			geom_timer.stop();
			auto dur=geom_timer.getMicros();
			std::cout<<"  geom: "<<dur<<"us ("<<(dur/1000.f)<<"ms)\n";
		}

		if(to_time) pc_timer.start();

		return true;
	}

	bool user_render() override {
		if(to_time) {
			pc_timer.stop();
			auto dur=pc_timer.getMicros();
			std::cout<<"  project & clip: "<<dur<<"us ("<<(dur/1000.f)<<"ms)\n";
		}

		if(to_time) render_timer.start();
		
		//dark grey background
		Clear(olc::Pixel(90, 90, 90));

		render3D();

		if(to_time) {
			render_timer.stop();
			auto dur=render_timer.getMicros();
			std::cout<<"  render: "<<dur<<"us ("<<(dur/1000.f)<<"ms)\n";
			to_time=false;
		}

		return true;
	}
};

int main() {
	BlockGame bg;
	if(bg.Construct(640, 480, 1, 1, false, true)) bg.Start();

	return 0;
}