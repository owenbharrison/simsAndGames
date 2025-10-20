#include "common/3d/engine_3d.h"
using cmn::vf3d;

#include "mesh.h"

struct PlanetGame : cmn::Engine3D {
	PlanetGame() {
		sAppName="Planet Game";
	}

	vf3d light_pos{0, 3, 3};

	vf3d planet_pos;
	float planet_rad=1;
	vf3d player_pos{0, 1, 0};
	vf3d player_fwd{1, 0, 0};
	vf3d player_up, player_rgt;

	Mesh planet, player;

	bool user_create() override {
		cam_pos={2, 2, 2};

		//try load meshes
		try {
			planet=Mesh::loadFromOBJ("assets/sphere.txt");
			player=Mesh::loadFromOBJ("assets/sphere.txt");
		} catch(const std::exception& e) {
			std::cerr<<"  "<<e.what();
			return false;
		}

		return true;
	}
	
#pragma region UPDATE HELPERS
	void handleCameraMovement(float dt) {
		float speed=3*dt;
		if(GetKey(olc::Key::UP).bHeld) cam_pos.z-=speed;
		if(GetKey(olc::Key::DOWN).bHeld) cam_pos.z+=speed;
		if(GetKey(olc::Key::LEFT).bHeld) cam_pos.x-=speed;
		if(GetKey(olc::Key::RIGHT).bHeld) cam_pos.x+=speed;
		if(GetKey(olc::Key::SHIFT).bHeld) cam_pos.y-=speed;
		if(GetKey(olc::Key::SPACE).bHeld) cam_pos.y+=speed;

		//point towards origin
		cam_dir=-cam_pos.norm();
	}

	void handlePlayerMovement(float dt) {
		player_up=(player_pos-planet_pos).norm();
		player_rgt=player_fwd.cross(player_up).norm();

		//walking forward & back
		const float walk_speed=1;
		{
			bool walk_fwd=GetKey(olc::Key::W).bHeld;
			bool walk_back=GetKey(olc::Key::S).bHeld;
			if(walk_fwd^walk_back) {
				//slow backwards
				float fb_modifier=walk_fwd?1:-.7f;
				float walk_amt=walk_speed*fb_modifier*dt;

				//move by forward dir
				player_pos+=player_fwd*walk_amt;

				//get new up dir
				player_up=(player_pos-planet_pos).norm();

				//reproject pos onto sphere...
				player_pos=planet_pos+planet_rad*player_up;

				//get new fwd dir (since rgt didnt change)
				player_fwd=player_up.cross(player_rgt);
			}
		}

		//strafing left & right
		{
			const float strafe_speed=.85f*walk_speed;
			bool strafe_left=GetKey(olc::Key::A).bHeld;
			bool strafe_right=GetKey(olc::Key::D).bHeld;
			if(strafe_left^strafe_right) {
				int lr_modifier=strafe_right?1:-1;
				float strafe_amt=strafe_speed*lr_modifier*dt;

				//move by rgt dir
				player_pos+=player_rgt*strafe_amt;

				//get new up dir
				player_up=(player_pos-planet_pos).norm();

				//reproject pos onto sphere...
				player_pos=planet_pos+planet_rad*player_up;

				//get new rgt dir (since fwd didnt change)
				player_rgt=player_fwd.cross(player_up);
			}
		}
		
		//turning left & right
		{
			const float turn_speed=2;
			bool turn_left=GetKey(olc::Key::Q).bHeld;
			bool turn_right=GetKey(olc::Key::E).bHeld;
			if(turn_right^turn_left) {
				int lr_modifier=turn_right?1:-1;
				float turn_amt=turn_speed*lr_modifier*dt;
				
				//get new fwd/rgt dirs (since up doesnt change)
				vf3d fwd_new=std::cos(turn_amt)*player_fwd+std::sin(turn_amt)*player_rgt;
				vf3d rgt_new=-std::sin(turn_amt)*player_fwd+std::cos(turn_amt)*player_rgt;
				player_fwd=fwd_new;
				player_rgt=rgt_new;
			}
		}
	}
#pragma endregion

	bool user_update(float dt) override {
		handleCameraMovement(dt);

		handlePlayerMovement(dt);
		
		return true;
	}

#pragma region GEOMETRY HELPERS
	//this looks nice :D
	void realizeArrow(const vf3d& a, const vf3d& b, float sz, const olc::Pixel& col) {
		vf3d ba=b-a;
		float mag=ba.mag();
		vf3d ca=cam_pos-a;
		vf3d perp=.5f*sz*mag*ba.cross(ca).norm();
		vf3d c=b-sz*ba;
		vf3d l=c-perp, r=c+perp;
		cmn::Line l1{a, c}; l1.col=col;
		lines_to_project.push_back(l1);
		cmn::Line l2{l, r}; l2.col=col;
		lines_to_project.push_back(l2);
		cmn::Line l3{l, b}; l3.col=col;
		lines_to_project.push_back(l3);
		cmn::Line l4{r, b}; l4.col=col;
		lines_to_project.push_back(l4);
	}

	//add planet mesh
	void realizePlanet(const olc::Pixel& col) {
		planet.translation=planet_pos;
		planet.scale=planet_rad*vf3d(1, 1, 1);
		planet.updateTransforms();
		planet.updateTriangles(col);
		tris_to_project.insert(tris_to_project.end(),
			planet.tris.begin(), planet.tris.end()
		);
	}

	//add player mesh
	void realizePlayer(float sz, const olc::Pixel& col) {
		planet.translation=player_pos;
		planet.scale=sz*vf3d(1, 1, 1);
		planet.updateTransforms();
		planet.updateTriangles(col);
		tris_to_project.insert(tris_to_project.end(),
			planet.tris.begin(), planet.tris.end()
		);
	}

	//add unit coordinate system
	void realizeAxes(const vf3d& pos, float sz) {
		realizeArrow(pos, pos+vf3d(sz, 0, 0), .2f, olc::RED);
		realizeArrow(pos, pos+vf3d(0, sz, 0), .2f, olc::BLUE);
		realizeArrow(pos, pos+vf3d(0, 0, sz), .2f, olc::GREEN);
	}

	//show player directions with arrows
	void realizePlayerCoordinateSystem(float sz) {
		realizeArrow(player_pos, player_pos+player_fwd, .2f, olc::YELLOW);//~z
		realizeArrow(player_pos, player_pos+player_up, .2f, olc::CYAN);//~y
		realizeArrow(player_pos, player_pos+player_rgt, .2f, olc::MAGENTA);//~x
	}
#pragma endregion

	bool user_geometry() override {
		//add main light
		lights.push_back({light_pos, olc::WHITE});

		realizePlanet(olc::WHITE);

		realizePlayer(.1f, olc::BLACK);

		realizeAxes({0, 0, 0}, 2);

		realizePlayerCoordinateSystem(.5f);

		return true;
	}

	bool user_render() override {
		Clear(olc::Pixel(90, 90, 90));

		resetBuffers();

		for(const auto& t:tris_to_draw) {
			FillDepthTriangle(
				t.p[0].x, t.p[0].y, t.t[0].w,
				t.p[1].x, t.p[1].y, t.t[1].w,
				t.p[2].x, t.p[2].y, t.t[2].w,
				t.col, t.id
			);
		}

		for(const auto& l:lines_to_draw) {
			DrawDepthLine(
				l.p[0].x, l.p[0].y, l.t[0].w,
				l.p[1].x, l.p[1].y, l.t[1].w,
				l.col, l.id
			);
		}

		return true;
	}
};

int main() {
	PlanetGame pg;
	if(pg.Construct(400, 400, 1, 1, false, true)) pg.Start();

	return 0;
}