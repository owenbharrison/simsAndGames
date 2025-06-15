#include "common/3d/engine_3d.h"
using cmn::vf3d;
using cmn::Mat4;

constexpr float Pi=3.1415927f;

struct SpriteSheet {
	olc::Sprite* sprite=nullptr;
	int num_x=0, num_y=0;
	std::unordered_map<std::string, std::vector<int>> animations;

	SpriteSheet() {}

	SpriteSheet(const std::string& filename, int x, int y) {
		sprite=new olc::Sprite(filename);
		num_x=x;
		num_y=y;
	}

	~SpriteSheet() {
		delete sprite;
	}
};

struct Billboard {
	vf3d pos;
	int id=0;
	std::string state;
	float anim=0;
	float w=1, h=1;
	float rot=0;

	void update(float dt) {
		anim+=dt;
	}
};

//pack four 0-255 uchars into an int
int pack(unsigned char a, unsigned char b, unsigned char c, unsigned char d) {
	return (a<<24)|(b<<16)|(c<<8)|d;
}

//retrieve four 0-255 uchars from an int
void unpack(int packed, unsigned char& a, unsigned char& b, unsigned char& c, unsigned char& d) {
	a=(packed>>24)&0xFF;
	b=(packed>>16)&0xFF;
	c=(packed>>8)&0xFF;
	d=packed&0xFF;
}

struct Example : cmn::Engine3D {
	Example() {
		sAppName="billboard spritesheets";
	}

	//camera positioning
	float cam_yaw=-Pi/2;
	float cam_pitch=0;

	std::vector<SpriteSheet*> sprite_sheet_atlas;
	std::list<Billboard> billboards;

	Billboard* selected=nullptr;

	bool user_create() override {
		cam_pos={0, 0, 2.5f};
		light_pos={0, 0, 2.5f};

		{
			//make actual sprite sheet
			SpriteSheet* four_ways=new SpriteSheet("assets/four_ways.png", 6, 4);
			four_ways->animations["backward"]={0, 1, 2, 3, 4, 5};
			four_ways->animations["left"]={6, 7, 8, 9, 10, 11};
			four_ways->animations["right"]={12, 13, 14, 15, 16, 17};
			four_ways->animations["forward"]={18, 19, 20, 21, 22, 23};
			sprite_sheet_atlas.push_back(four_ways);
		}

		{
			//make indexed sprite sheet
			SpriteSheet* indexing=new SpriteSheet("assets/indexing.png", 6, 4);
			indexing->animations["backward"]={0, 1, 2, 3, 4, 5};
			indexing->animations["left"]={6, 7, 8, 9, 10, 11};
			indexing->animations["right"]={12, 13, 14, 15, 16, 17};
			indexing->animations["forward"]={18, 19, 20, 21, 22, 23};
			sprite_sheet_atlas.push_back(indexing);
		}

		//pack(id, sprite_sheet, n/a, n/a)
		//billboards with actual sprite sheet
		billboards.push_back({vf3d(-3, -1, 0), pack(0, 0, 0, 0), "forward"});
		billboards.push_back({vf3d(-1, -1, 0), pack(1, 0, 0, 0), "backward"});
		billboards.push_back({vf3d(1, -1, 0), pack(2, 0, 0, 0), "left"});
		billboards.push_back({vf3d(3, -1, 0), pack(3, 0, 0, 0), "right"});

		//billboards with indexed sprite sheet
		billboards.push_back({vf3d(-3, 1, 0), pack(4, 1, 0, 0), "forward"});
		billboards.push_back({vf3d(-1, 1, 0), pack(5, 1, 0, 0), "backward"});
		billboards.push_back({vf3d(1, 1, 0), pack(6, 1, 0, 0), "left"});
		billboards.push_back({vf3d(3, 1, 0), pack(7, 1, 0, 0), "right"});

		return true;
	}

	bool user_destroy() override {
		for(auto& s:sprite_sheet_atlas) delete s;

		return true;
	}

	bool user_update(float dt) override {
		//look up, down
		if(GetKey(olc::Key::UP).bHeld) cam_pitch+=dt;
		if(cam_pitch>Pi/2) cam_pitch=Pi/2-.001f;
		if(GetKey(olc::Key::DOWN).bHeld) cam_pitch-=dt;
		if(cam_pitch<-Pi/2) cam_pitch=.001f-Pi/2;

		//look left, right
		if(GetKey(olc::Key::LEFT).bHeld) cam_yaw-=dt;
		if(GetKey(olc::Key::RIGHT).bHeld) cam_yaw+=dt;

		//polar to cartesian
		cam_dir=vf3d(
			std::cos(cam_yaw)*std::cos(cam_pitch),
			std::sin(cam_pitch),
			std::sin(cam_yaw)*std::cos(cam_pitch)
		);

		//move up, down
		if(GetKey(olc::Key::SPACE).bHeld) cam_pos.y+=4.f*dt;
		if(GetKey(olc::Key::SHIFT).bHeld) cam_pos.y-=4.f*dt;

		//move forward, backward
		vf3d fb_dir(std::cos(cam_yaw), 0, std::sin(cam_yaw));
		if(GetKey(olc::Key::W).bHeld) cam_pos+=5.f*dt*fb_dir;
		if(GetKey(olc::Key::S).bHeld) cam_pos-=3.f*dt*fb_dir;

		//move left, right
		vf3d lr_dir(fb_dir.z, 0, -fb_dir.x);
		if(GetKey(olc::Key::A).bHeld) cam_pos+=4.f*dt*lr_dir;
		if(GetKey(olc::Key::D).bHeld) cam_pos-=4.f*dt*lr_dir;

		//set light pos
		if(GetKey(olc::Key::L).bHeld) light_pos=cam_pos;

		//deselect
		if(GetKey(olc::Key::ESCAPE).bPressed) selected=nullptr;

		//select billboard with id buffer
		if(GetKey(olc::Key::ENTER).bPressed) {
			//get index
			int cid=id_buffer[bufferIX(ScreenWidth()/2, ScreenHeight()/2)];

			//unpack ids and compare
			selected=nullptr;
			for(auto& b:billboards) {
				unsigned char id, _na;
				unpack(b.id, id, _na, _na, _na);
				if(id==cid) {
					selected=&b;
					break;
				}
			}
		}

		//rotate billboard
		if(selected) {
			if(GetKey(olc::Key::Q).bHeld) selected->rot+=dt;
			if(GetKey(olc::Key::E).bHeld) selected->rot-=dt;
		}

		//basically just updating the animation
		for(auto& b:billboards) {
			b.update(dt);
		}

		return true;
	}

	bool user_geometry() override {
		//sort billboards by camera dist for transparency
		billboards.sort([&] (const Billboard& a, const Billboard& b) {
			return (a.pos-cam_pos).mag2()>(b.pos-cam_pos).mag2();
		});

		//add billboards
		for(const auto& b:billboards) {
			//unpack sprite sheet index
			unsigned char ss_idx, _na;
			unpack(b.id, _na, ss_idx, _na, _na);

			//get basis vectors
			vf3d norm=(b.pos-cam_pos).norm();
			vf3d up(0, 1, 0);
			vf3d rgt=norm.cross(up).norm();
			up=rgt.cross(norm);

			//rotate basis vectors w/ rotation matrix
			float c=std::cos(b.rot), s=std::sin(b.rot);
			vf3d new_rgt=c*rgt+s*up;
			vf3d new_up=-s*rgt+c*up;

			//vertex positioning
			vf3d tl=b.pos+b.w/2*new_rgt+b.h/2*new_up;
			vf3d tr=b.pos+b.w/2*new_rgt-b.h/2*new_up;
			vf3d bl=b.pos-b.w/2*new_rgt+b.h/2*new_up;
			vf3d br=b.pos-b.w/2*new_rgt-b.h/2*new_up;

			//get corresponding sprite sheet
			SpriteSheet& ss=*sprite_sheet_atlas[ss_idx];
			//get corresponding list of poses
			const auto& poses=ss.animations[b.state];
			//wrap animation step and get pose
			int pose=poses[int(b.anim)%poses.size()];
			//find texture region x
			int pose_x=pose%ss.num_x;
			float min_u=pose_x/float(ss.num_x);
			float max_u=min_u+1.f/ss.num_x;
			//find texture region y
			int pose_y=pose/ss.num_x;
			float min_v=pose_y/float(ss.num_y);
			float max_v=min_v+1.f/ss.num_y;

			//texture using given region
			cmn::v2d tl_t{max_u, min_v};
			cmn::v2d tr_t{max_u, max_v};
			cmn::v2d bl_t{min_u, min_v};
			cmn::v2d br_t{min_u, max_v};

			//tesselate vertexes and texture coords and pass packed int
			cmn::Triangle f1{tl, br, tr, tl_t, br_t, tr_t}; f1.id=b.id;
			tris_to_project.push_back(f1);
			cmn::Triangle f2{tl, bl, br, tl_t, bl_t, br_t}; f2.id=b.id;
			tris_to_project.push_back(f2);
		}

		return true;
	}

	bool user_render() override {
		Clear(olc::DARK_GREY);

		//render 3d stuff
		resetBuffers();

		SetPixelMode(olc::Pixel::Mode::ALPHA);
		for(const auto& t:tris_to_draw) {
			//unpack id and sprite sheet index
			unsigned char id, ss_idx, _na;
			unpack(t.id, id, ss_idx, _na, _na);
			//texture with corresponding sprite lookup
			FillTexturedDepthTriangle(
				t.p[0].x, t.p[0].y, t.t[0].u, t.t[0].v, t.t[0].w,
				t.p[1].x, t.p[1].y, t.t[1].u, t.t[1].v, t.t[1].w,
				t.p[2].x, t.p[2].y, t.t[2].u, t.t[2].v, t.t[2].w,
				sprite_sheet_atlas[ss_idx]->sprite, t.col, id
			);
		}
		SetPixelMode(olc::Pixel::Mode::NORMAL);

		//draw crosshair
		const int sz=8;
		int cx=ScreenWidth()/2, cy=ScreenHeight()/2;
		DrawLine(cx-sz, cy, cx+sz, cy, olc::WHITE);
		DrawLine(cx, cy-sz, cx, cy+sz, olc::WHITE);

		//highlight selected
		if(selected) {
			unsigned char id, _na;
			unpack(selected->id, id, _na, _na, _na);
			for(int i=1; i<ScreenWidth()-1; i++) {
				for(int j=1; j<ScreenHeight()-1; j++) {
					bool curr=id_buffer[i+ScreenWidth()*j]==id;
					bool lft=id_buffer[i-1+ScreenWidth()*j]==id;
					bool rgt=id_buffer[i+1+ScreenWidth()*j]==id;
					bool top=id_buffer[i+ScreenWidth()*(j-1)]==id;
					bool btm=id_buffer[i+ScreenWidth()*(j+1)]==id;
					if(curr!=lft||curr!=rgt||curr!=top||curr!=btm) {
						Draw(i, j, olc::BLUE);
					}
				}
			}
		}

		return true;
	}
};

int main() {
	Example e;
	if(e.Construct(640, 320, 1, 1, false, true)) e.Start();

	return 0;
}