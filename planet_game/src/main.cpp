#define OLC_PGE_APPLICATION
#include "common/3d/engine_3d.h"
namespace olc {
	static const Pixel PURPLE(144, 0, 255);
}
using cmn::vf3d;

#include "mesh.h"

constexpr float Pi=3.1415927f;

struct PlanetGame : cmn::Engine3D {
	PlanetGame() {
		sAppName="Planet Game";
	}

	vf3d light_pos;

	//ambiguous
	vf3d camera_pos;
	vf3d camera_dir;

	vf3d planet_pos;
	const float planet_rad=2;

	vf3d player_pos;
	vf3d player_fwd;
	vf3d player_up, player_rgt;
	float player_pitch=0;
	vf3d player_look;
	const float player_sz=.1f;

	Mesh planet, player;

	bool show_info=false;
	bool player_perspective=false;

	bool user_create() override {
		light_pos=(1+planet_rad)*vf3d(0, 1, 1);
		camera_pos=(1+planet_rad)*vf3d(1, 1, 1);

		player_pos={0, planet_rad, 0};
		player_fwd={1, 0, 0};

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
	//unintuitive
	void handleCameraMovement(float dt) {
		float speed=3*dt;
		if(GetKey(olc::Key::F).bHeld) camera_pos.x-=speed;
		if(GetKey(olc::Key::H).bHeld) camera_pos.x+=speed;
		if(GetKey(olc::Key::R).bHeld) camera_pos.y-=speed;
		if(GetKey(olc::Key::Y).bHeld) camera_pos.y+=speed;
		if(GetKey(olc::Key::T).bHeld) camera_pos.z-=speed;
		if(GetKey(olc::Key::G).bHeld) camera_pos.z+=speed;

		//point towards origin
		camera_dir=-camera_pos.norm();
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
				float fb_modifier=walk_fwd?1:-.5f;
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
			const float strafe_speed=.6f*walk_speed;
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
			bool turn_left=GetKey(olc::Key::LEFT).bHeld;
			bool turn_right=GetKey(olc::Key::RIGHT).bHeld;
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

		//looking up & down
		{
			const float look_speed=2;
			bool look_up=GetKey(olc::Key::UP).bHeld;
			bool look_down=GetKey(olc::Key::DOWN).bHeld;
			if(look_up^look_down) {
				int ud_modifier=look_up?1:-1;
				float look_amt=look_speed*ud_modifier*dt;

				//get new pitch
				player_pitch+=look_amt;
				if(player_pitch<-Pi/2) player_pitch=.001f-Pi/2;
				if(player_pitch>Pi/2) player_pitch=Pi/2-.001f;
			}
		}

		//get new look dir
		player_look=std::cos(player_pitch)*player_fwd+std::sin(player_pitch)*player_up;
	}
#pragma endregion

	bool user_update(float dt) override {
		handleCameraMovement(dt);

		handlePlayerMovement(dt);

		//graphics toggles
		if(GetKey(olc::Key::I).bPressed) show_info^=true;
		if(GetKey(olc::Key::P).bPressed) player_perspective^=true;
		if(GetKey(olc::Key::L).bPressed) light_pos=camera_pos;

		if(player_perspective) {
			cam_pos=player_pos+player_sz*player_up;
			cam_dir=player_look;
			cam_up=player_up;
		} else {
			cam_pos=camera_pos;
			cam_dir=camera_dir;
			cam_up={0, 1, 0};
		}

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
	
	//add unit coordinate system
	void realizeAxes(const vf3d& pos, float sz) {
		realizeArrow(pos, pos+vf3d(sz, 0, 0), .1f, olc::RED);
		realizeArrow(pos, pos+vf3d(0, sz, 0), .1f, olc::BLUE);
		realizeArrow(pos, pos+vf3d(0, 0, sz), .1f, olc::GREEN);
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

	//show player directions with arrows
	void realizePlayerCoordinateSystem(float sz) {
		realizeArrow(player_pos, player_pos+sz*player_look, .2f, olc::BLACK);
		realizeArrow(player_pos, player_pos+sz*player_rgt, .2f, olc::MAGENTA);//~x
		realizeArrow(player_pos, player_pos+sz*player_up, .2f, olc::CYAN);//~y
		realizeArrow(player_pos, player_pos+sz*player_fwd, .2f, olc::YELLOW);//~z
	}
#pragma endregion

	bool user_geometry() override {
		//add main light
		lights.push_back({light_pos, olc::WHITE});

		realizeAxes({0, 0, 0}, 1+planet_rad);

		realizePlanet(olc::WHITE);

		if(!player_perspective) {
			realizePlayer(.075f, olc::PURPLE);
			realizePlayerCoordinateSystem(.33f);
		} else {
			//show axes right in front of player
			if(show_info) realizeAxes(cam_pos+player_sz*player_look, .25f*player_sz);
		}

		return true;
	}

	//helpful strings in corners of screen
	void renderInfo() {

		DrawString(0, 0, "player controls");
		DrawString(0, 8, "W/S: walk");
		DrawString(0, 16, "A/D: strafe");
		DrawString(0, 24, "LEFT/RIGHT: turn");
		DrawString(0, 32, "UP/DOWN: look");

		DrawString(ScreenWidth()-8*15, 0, "camera controls");
		DrawString(ScreenWidth()-8*14, 8, "F/H: move in x");
		DrawString(ScreenWidth()-8*14, 16, "R/Y: move in y");
		DrawString(ScreenWidth()-8*14, 24, "T/G: move in z");

		DrawString(0, ScreenHeight()-32, "player axes");
		DrawString(0, ScreenHeight()-24, "rgt(~x)", olc::MAGENTA);
		DrawString(0, ScreenHeight()-16, "up(~y)", olc::CYAN);
		DrawString(0, ScreenHeight()-8, "fwd(~z)", olc::YELLOW);

		DrawString(ScreenWidth()-8*10, ScreenHeight()-32, "world axes");
		DrawString(ScreenWidth()-8*6, ScreenHeight()-24, "rgt(x)", olc::RED);
		DrawString(ScreenWidth()-8*5, ScreenHeight()-16, "up(y)", olc::BLUE);
		DrawString(ScreenWidth()-8*6, ScreenHeight()-8, "fwd(z)", olc::GREEN);
	}

	//"which keys can i press?"
	void renderHints() {
		int y=ScreenHeight();
		if(!player_perspective) {
			y-=8;
			DrawString(ScreenWidth()/2-4*33, y, "[press P for player perspective]");
		}

		if(show_info) renderInfo();
		else {
			y-=8;
			DrawString(ScreenWidth()/2-4*18, y, "[press I for info]");
		}
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

		renderHints();

		return true;
	}
};

int main() {
	PlanetGame pg;
	if(pg.Construct(640, 480, 1, 1, false, true)) pg.Start();

	return 0;
}