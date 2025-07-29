/*TODO
use vi3d more?

explosion particles

saving/loading

add difficulty modes
add game timer
*/

#define OLC_GFX_OPENGL33
#include "common/3d/engine_3d.h"
using cmn::vf3d;
using cmn::Mat4;

constexpr float Pi=3.1415927f;

struct Message {
	std::string str;
	olc::Pixel col;
	float lifespan=0;
	float age=0;
};

#include "mesh.h"

struct Billboard {
	vf3d pos;
	float size=1;
	float ax=.5f, ay=.5f;
	int spr_ix=0;
	olc::Pixel col=olc::WHITE;
	int i=0, j=0, k=0;
};

#include "thread_pool.h"

#include "minesweeper.h"

struct Minesweeper3DUI : cmn::Engine3D {
	Minesweeper3DUI() {
		sAppName="3D Minesweeper";
	}

	//camera direction
	float cam_yaw=1.07f;
	float cam_pitch=-.56f;

	//orbit controls
	olc::vf2d* orbit_start=nullptr;
	float temp_yaw, temp_pitch;
	float cam_rad=0;

	//cursor controls
	bool show_controls=false;
	std::vector<Message> messages;
	int cursor_i=0, cursor_j=0, cursor_k=0;
	bool bomb_cursor=false;

	//geometry stuff
	Mesh cursor_mesh;
	std::vector<Billboard> billboards;
	olc::Sprite* gradient=nullptr;
	int letter_ix=0, number_ix=0, bomb_ix=0, tile_ix=0, flag_ix=0;

	//graphics things
	std::vector<olc::Sprite*> texture_atlas;

	//multithreaded rendering
	const int mtr_tile_size=32;
	int mtr_num_x=0, mtr_num_y=0;
	std::vector<cmn::Triangle>* mtr_bins=nullptr;
	std::vector<olc::vi2d> mtr_jobs;
	ThreadPool* mtr_pool=nullptr;

	//game things
	Minesweeper* game=nullptr;
	vf3d game_size;

	void game_updateMesh() {
		game->triangulateUnswept(tile_ix, flag_ix);
	}

	bool user_create() override {
		srand(time(0));

		light_pos={10, 20, 30};

		//load gradient
		gradient=new olc::Sprite("assets/img/gradient.png");

		//load cursor model
		try {
			cursor_mesh=Mesh::loadFromOBJ("assets/models/cursor.txt");
		} catch(const std::exception& e) {
			std::cout<<e.what()<<'\n';
			return false;
		}

#pragma region TEXTURE ATLAS SETUP
		//add letters to texture atlas
		letter_ix=texture_atlas.size();
		for(char c='A'; c<='Z'; c++) {
			olc::Sprite* spr=new olc::Sprite(8, 8);
			SetDrawTarget(spr);

			Clear(olc::BLANK);
			DrawString(0, 0, std::string(1, c), olc::WHITE);

			texture_atlas.push_back(spr);
		}
		SetDrawTarget(nullptr);

		//add numbers to texture atlas
		number_ix=texture_atlas.size();
		for(int i=0; i<=9; i++) {
			olc::Sprite* spr=new olc::Sprite(8, 8);
			SetDrawTarget(spr);

			Clear(olc::BLANK);
			DrawString(0, 0, std::to_string(i), olc::WHITE);

			texture_atlas.push_back(spr);
		}
		SetDrawTarget(nullptr);

		//add tile, flag, & bomb to texture atlas
		tile_ix=texture_atlas.size();
		texture_atlas.push_back(new olc::Sprite("assets/img/tile.png"));

		flag_ix=texture_atlas.size();
		texture_atlas.push_back(new olc::Sprite("assets/img/flag_tile.png"));

		bomb_ix=texture_atlas.size();
		texture_atlas.push_back(new olc::Sprite("assets/img/bomb.png"));
#pragma endregion

#pragma region MULTI THREADING RENDERING SETUP
		//allocate tiles
		mtr_num_x=1+ScreenWidth()/mtr_tile_size;
		mtr_num_y=1+ScreenHeight()/mtr_tile_size;
		mtr_bins=new std::vector<cmn::Triangle>[mtr_num_x*mtr_num_y];

		//allocate jobs
		for(int i=0; i<mtr_num_x; i++) {
			for(int j=0; j<mtr_num_y; j++) {
				mtr_jobs.push_back({i, j});
			}
		}

		//make thread pool
		mtr_pool=new ThreadPool(std::thread::hardware_concurrency());
#pragma endregion

#pragma region GAME SETUP
		//init game
		try {
			game=new Minesweeper(5, 5, 5, 6);
		} catch(const std::exception& e) {
			std::cout<<e.what()<<'\n';
			return false;
		}
		game_updateMesh();

		//camera orbit rad based on game size
		game_size=vf3d(game->width, game->height, game->depth);
		cam_rad=1.5f+.5f*std::sqrtf(
			game->width*game->width+
			game->height*game->height+
			game->depth*game->depth
		);
#pragma endregion

		return true;
	}

	bool user_destroy() override {
		for(auto& t:texture_atlas) delete t;
		delete gradient;

		delete[] mtr_bins;
		delete mtr_pool;

		delete game;

		return true;
	}

	bool user_update(float dt) override {
		//look up, down
		if(GetKey(olc::Key::UP).bHeld) cam_pitch-=dt;
		if(GetKey(olc::Key::DOWN).bHeld) cam_pitch+=dt;
		//clamp pitch
		if(cam_pitch<-Pi/2) cam_pitch=.001f-Pi/2;
		if(cam_pitch>Pi/2) cam_pitch=Pi/2-.001f;

		//look left, right
		if(GetKey(olc::Key::LEFT).bHeld) cam_yaw+=dt;
		if(GetKey(olc::Key::RIGHT).bHeld) cam_yaw-=dt;

		//start orbit
		if(GetMouse(olc::Mouse::LEFT).bPressed) {
			orbit_start=new olc::vf2d(GetMousePos());
		}

		//temporary orbit direction
		temp_pitch=cam_pitch;
		temp_yaw=cam_yaw;
		if(orbit_start) {
			olc::vf2d diff=GetMousePos()-*orbit_start;
			temp_pitch-=.01f*diff.y;
			//clamp new pitch
			if(temp_pitch<-Pi/2) temp_pitch=.001f-Pi/2;
			if(temp_pitch>Pi/2) temp_pitch=Pi/2-.001f;
			temp_yaw+=.01f*diff.x;
		}

		//end orbit
		if(GetMouse(olc::Mouse::LEFT).bReleased) {
			//set actual direction to temp direction
			cam_yaw=temp_yaw;
			cam_pitch=temp_pitch;

			//clear pointer flags
			delete orbit_start;
			orbit_start=nullptr;
		}

		//polar to cartesian
		cam_dir=vf3d(
			std::cosf(temp_yaw)*std::cosf(temp_pitch),
			std::sinf(temp_pitch),
			std::sinf(temp_yaw)*std::cosf(temp_pitch)
		);

		//cam always points toward origin
		cam_pos=-cam_rad*cam_dir;
		//light always at cam
		light_pos=cam_pos;

		//toggle cursor control display
		if(GetKey(olc::Key::C).bPressed) {
			show_controls^=true;

			std::string msg="cursor control display ";
			olc::Pixel col;
			if(show_controls) msg+="ON", col=olc::GREEN;
			else msg+="OFF", col=olc::RED;
			messages.push_back({msg, col, 2});
		}

		//toggle bomb zone display
		if(GetKey(olc::Key::B).bPressed) {
			bomb_cursor^=true;

			std::string msg="bomb influence zone ";
			olc::Pixel col;
			if(bomb_cursor) msg+="ON", col=olc::CYAN;
			else msg+="OFF", col=olc::MAGENTA;
			messages.push_back({msg, col, 2});
		}

		//move cursor with keyboard
		{
			if(GetKey(olc::Key::A).bPressed) cursor_i++;
			if(GetKey(olc::Key::D).bPressed) cursor_i--;
			if(GetKey(olc::Key::W).bPressed) cursor_j++;
			if(GetKey(olc::Key::S).bPressed) cursor_j--;
			if(GetKey(olc::Key::Q).bPressed) cursor_k++;
			if(GetKey(olc::Key::E).bPressed) cursor_k--;

			//cursor bounds
			if(cursor_i<0) cursor_i=0;
			if(cursor_j<0) cursor_j=0;
			if(cursor_k<0) cursor_k=0;
			if(cursor_i>=game->width) cursor_i=game->width-1;
			if(cursor_j>=game->height) cursor_j=game->height-1;
			if(cursor_k>=game->depth) cursor_k=game->depth-1;
		}

		//set things at mouse
		if(GetMouse(olc::Mouse::LEFT).bPressed) {
			//previous frame's id buffer, but its no big deal
			int id=id_buffer[bufferIX(GetMouseX(), GetMouseY())];
			int type=0xff&(id>>24);
			switch(type) {
				case 1://cursor to cell
					cursor_i=31&(id>>10);
					cursor_j=31&(id>>5);
					cursor_k=31&id;
					break;
				case 2://cursor to billboard
					int bb_ix=0xffff&id;
					//previous frame's billboard list, but its no big deal
					const auto& b=billboards[bb_ix];
					cursor_i=b.i;
					cursor_j=b.j;
					cursor_k=b.k;
					break;
			}
		}

		//flag at cursor
		if(GetKey(olc::Key::F).bPressed) {
			game->flag(cursor_i, cursor_j, cursor_k);
			game_updateMesh();
		}

		//sweep at cursor
		if(GetKey(olc::Key::ENTER).bPressed) {
			game->sweep(cursor_i, cursor_j, cursor_k);
			game_updateMesh();
		}

		//reset game
		if(GetKey(olc::Key::R).bPressed) {
			game->reset();
			game_updateMesh();
		}

		//update & remove "dead" messages
		for(auto it=messages.begin(); it!=messages.end();) {
			it->age+=dt;
			if(it->age>it->lifespan) it=messages.erase(it);
			else it++;
		}

		return true;
	}

#pragma region GEOMETRY HELPERS
	void makeQuad(const vf3d& pos, float sz, float ax, float ay, cmn::Triangle& a, cmn::Triangle& b) {
		//billboarded to point at camera
		vf3d norm=(pos-cam_pos).norm();
		vf3d up(0, 1, 0);
		vf3d rgt=norm.cross(up).norm();
		up=rgt.cross(norm);

		//vertex positioning
		vf3d tl=pos-sz*ax*rgt+sz*ay*up;
		vf3d tr=pos+sz*(1-ax)*rgt+sz*ay*up;
		vf3d bl=pos-sz*ax*rgt-sz*(1-ay)*up;
		vf3d br=pos+sz*(1-ax)*rgt-sz*(1-ay)*up;

		//texture coords
		cmn::v2d tl_t{0, 0};
		cmn::v2d tr_t{1, 0};
		cmn::v2d bl_t{0, 1};
		cmn::v2d br_t{1, 1};

		//tessellation
		a={tl, br, tr, tl_t, br_t, tr_t};
		b={tl, bl, br, tl_t, bl_t, br_t};
	}

	//this looks nice :D
	void addArrow(const vf3d& a, const vf3d& b, float sz, const olc::Pixel& col) {
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
#pragma endregion

	bool user_geometry() override {
		//show unswept
		tris_to_project.insert(tris_to_project.end(),
			game->unswept_tris.begin(), game->unswept_tris.end()
		);

		billboards.clear();

		//add neighbor counts as billboards
		for(int i=0; i<game->width; i++) {
			for(int j=0; j<game->height; j++) {
				for(int k=0; k<game->depth; k++) {
					const auto& cell=game->cells[game->ix(i, j, k)];

					//skip unswept
					if(!cell.swept) continue;

					//positioning
					vf3d pos=.5f+vf3d(i, j, k)-game_size/2;

					//add bomb sprite
					if(cell.bomb) {
						billboards.push_back({pos, .5f, .5f, .5f, bomb_ix, olc::WHITE, i, j, k});
						continue;
					}

					//skip empties
					if(cell.num_bombs==0) continue;

					//size if cursor on billboard
					float size=.35f;
					if(i==cursor_i&&j==cursor_j&&k==cursor_k) size=.6f;

					//display 2or1 digit number
					int tens=cell.num_bombs/10;
					int ones=cell.num_bombs%10;

					//color based on cell value
					olc::Pixel col;
					switch(cell.num_bombs) {
						case 1: col=olc::Pixel(0, 100, 255); break;//light blue
						case 2: col=olc::Pixel(0, 200, 0); break;//dark green
						case 3: col=olc::Pixel(220, 220, 0); break;//dark yellow
						case 4: col=olc::RED; break;
						case 5: col=olc::DARK_RED; break;
						default: {//6-26
							float t=(cell.num_bombs-6.f)/(26-6);
							col=gradient->Sample(t, 0);
							break;
						}
					}
					if(tens) {
						//different anchoring if 2digit
						billboards.push_back({pos, size, 1, .5f, number_ix+tens, col, i, j, k});
						billboards.push_back({pos, size, 0, .5f, number_ix+ones, col, i, j, k});
					} else {
						billboards.push_back({pos, size, .5f, .5f, number_ix+ones, col, i, j, k});
					}
				}
			}
		}

		//add cursor controls as arrows and billboards
		if(show_controls) {
			//a&d on x axis
			float edge_x=.5f*game->width;
			billboards.push_back({vf3d(2+edge_x, 0, 0), .5f, .5f, .5f, letter_ix+'A'-'A', olc::YELLOW});
			addArrow(vf3d(.5f+edge_x, 0, 0), vf3d(1.5f+edge_x, 0, 0), .15f, olc::YELLOW);
			billboards.push_back({vf3d(-2-edge_x, 0, 0), .5f, .5f, .5f, letter_ix+'D'-'A', olc::YELLOW});
			addArrow(vf3d(-.5f-edge_x, 0, 0), vf3d(-1.5f-edge_x, 0, 0), .15f, olc::YELLOW);
			//w&s on y axis
			float edge_y=.5f*game->height;
			billboards.push_back({vf3d(0, 2+edge_y, 0), .5f, .5f, .5f, letter_ix+'W'-'A', olc::MAGENTA});
			addArrow(vf3d(0, .5f+edge_y, 0), vf3d(0, 1.5f+edge_y, 0), .15f, olc::MAGENTA);
			billboards.push_back({vf3d(0, -2-edge_y, 0), .5f, .5f, .5f, letter_ix+'S'-'A', olc::MAGENTA});
			addArrow(vf3d(0, -.5f-edge_y, 0), vf3d(0, -1.5f-edge_y, 0), .15f, olc::MAGENTA);
			//q&e on z axis
			float edge_z=.5f*game->depth;
			billboards.push_back({vf3d(0, 0, 2+edge_z), .5f, .5f, .5f, letter_ix+'Q'-'A', olc::GREEN});
			addArrow(vf3d(0, 0, .5f+edge_z), vf3d(0, 0, 1.5f+edge_z), .15f, olc::GREEN);
			billboards.push_back({vf3d(0, 0, -2-edge_z), .5f, .5f, .5f, letter_ix+'E'-'A', olc::GREEN});
			addArrow(vf3d(0, 0, -.5f-edge_z), vf3d(0, 0, -1.5f-edge_z), .15f, olc::GREEN);
		}

		//sort billboards
		std::sort(billboards.begin(), billboards.end(), [&] (const Billboard& a, const Billboard& b) {
			return (a.pos-cam_pos).mag2()>(b.pos-cam_pos).mag2();
		});

		//tessellate billboards
		cmn::Triangle t1, t2;
		for(int i=0; i<billboards.size(); i++) {
			auto& b=billboards[i];
			makeQuad(b.pos, b.size, b.ax, b.ay, t1, t2);
			//pack type=2, spr_ix, and billboard index
			t1.id=t2.id=(2<<24)|(b.spr_ix<<16)|i;
			tris_to_project.push_back(t1);
			tris_to_project.push_back(t2);
		}

		//add bounds
		addAABB({-game_size/2, game_size/2}, olc::BLACK);

		//add cursor mesh
		{
			vf3d ijk(cursor_i, cursor_j, cursor_k);
			cursor_mesh.translation=.5f+ijk-game_size/2;
			cursor_mesh.updateTransforms();
			cursor_mesh.updateTriangles(olc::RED);
			tris_to_project.insert(tris_to_project.end(),
				cursor_mesh.tris.begin(), cursor_mesh.tris.end()
			);
		}

		if(bomb_cursor) {
			//show clamped 3x3 bomb influence zone
			int min_i=cursor_i-1; if(min_i<0) min_i=0;
			int min_j=cursor_j-1; if(min_j<0) min_j=0;
			int min_k=cursor_k-1; if(min_k<0) min_k=0;
			int max_i=cursor_i+2; if(max_i>=game->width) max_i=game->width;
			int max_j=cursor_j+2; if(max_j>=game->height) max_j=game->height;
			int max_k=cursor_k+2; if(max_k>=game->depth) max_k=game->depth;
			vf3d min_ijk(min_i, min_j, min_k);
			vf3d max_ijk(max_i, max_j, max_k);
			addAABB({min_ijk-game_size/2-.05f, max_ijk-game_size/2+.05f}, olc::Pixel(255, 120, 0));
		}

		return true;
	}

#pragma region RENDER HELPERS
	void FillDepthTriangleWithin(
		int x1, int y1, float w1,
		int x2, int y2, float w2,
		int x3, int y3, float w3,
		olc::Pixel col, int id,
		int nx, int ny, int mx, int my
	) {
		//sort by y
		if(y2<y1) {
			std::swap(x1, x2);
			std::swap(y1, y2);
			std::swap(w1, w2);
		}
		if(y3<y1) {
			std::swap(x1, x3);
			std::swap(y1, y3);
			std::swap(w1, w3);
		}
		if(y3<y2) {
			std::swap(x2, x3);
			std::swap(y2, y3);
			std::swap(w2, w3);
		}

		//calculate slopes
		int dx1=x2-x1;
		int dy1=y2-y1;
		float dw1=w2-w1;

		int dx2=x3-x1;
		int dy2=y3-y1;
		float dw2=w3-w1;

		int si, ei, sj, ej;

		float t;

		float tex_w;

		float dax_step=0, dbx_step=0,
			dw1_step=0, dw2_step=0;

		if(dy1) dax_step=dx1/std::fabsf(dy1);
		if(dy2) dbx_step=dx2/std::fabsf(dy2);

		if(dy1) dw1_step=dw1/std::fabsf(dy1);
		if(dy2) dw2_step=dw2/std::fabsf(dy2);

		if(dy1) for(int j=y1; j<=y2; j++) {
			if(j<ny||j>=my) continue;
			int ax=x1+dax_step*(j-y1);
			int bx=x1+dbx_step*(j-y1);
			float tex_sw=w1+dw1_step*(j-y1);
			float tex_ew=w1+dw2_step*(j-y1);
			//sort along x
			if(ax>bx) {
				std::swap(ax, bx);
				std::swap(tex_sw, tex_ew);
			}
			float t_step=1.f/(bx-ax);
			for(int i=ax; i<bx; i++) {
				if(i<nx||i>=mx) continue;
				t=t_step*(i-ax);
				tex_w=tex_sw+t*(tex_ew-tex_sw);
				float& depth=depth_buffer[bufferIX(i, j)];
				if(tex_w>depth) {
					Draw(i, j, col);
					depth=tex_w;
					id_buffer[bufferIX(i, j)]=id;
				}
			}
		}

		//recalculate slopes
		dx1=x3-x2;
		dy1=y3-y2;
		dw1=w3-w2;

		if(dy1) dax_step=dx1/std::fabsf(dy1);

		if(dy1) dw1_step=dw1/std::fabsf(dy1);

		for(int j=y2; j<=y3; j++) {
			if(j<ny||j>=my) continue;
			int ax=x2+dax_step*(j-y2);
			int bx=x1+dbx_step*(j-y1);
			float tex_sw=w2+dw1_step*(j-y2);
			float tex_ew=w1+dw2_step*(j-y1);
			//sort along x
			if(ax>bx) {
				std::swap(ax, bx);
				std::swap(tex_sw, tex_ew);
			}
			float t_step=1.f/(bx-ax);
			for(int i=ax; i<bx; i++) {
				if(i<nx||i>=mx) continue;
				t=t_step*(i-ax);
				tex_w=tex_sw+t*(tex_ew-tex_sw);
				float& depth=depth_buffer[bufferIX(i, j)];
				if(tex_w>depth) {
					Draw(i, j, col);
					depth=tex_w;
					id_buffer[bufferIX(i, j)]=id;
				}
			}
		}
	}

	void FillTexturedDepthTriangleWithin(
		int x1, int y1, float u1, float v1, float w1,
		int x2, int y2, float u2, float v2, float w2,
		int x3, int y3, float u3, float v3, float w3,
		const olc::Sprite& spr, olc::Pixel tint, int id,
		int nx, int ny, int mx, int my
	) {
		//sort by y
		if(y2<y1) {
			std::swap(x1, x2), std::swap(y1, y2);
			std::swap(u1, u2), std::swap(v1, v2);
			std::swap(w1, w2);
		}
		if(y3<y1) {
			std::swap(x1, x3), std::swap(y1, y3);
			std::swap(u1, u3), std::swap(v1, v3);
			std::swap(w1, w3);
		}
		if(y3<y2) {
			std::swap(x2, x3), std::swap(y2, y3);
			std::swap(u2, u3), std::swap(v2, v3);
			std::swap(w2, w3);
		}

		//calculate slopes
		int dx1=x2-x1, dy1=y2-y1;
		float du1=u2-u1, dv1=v2-v1;
		float dw1=w2-w1;

		int dx2=x3-x1, dy2=y3-y1;
		float du2=u3-u1, dv2=v3-v1;
		float dw2=w3-w1;

		float t_step, t;

		float tex_u, tex_v, tex_w;

		float dax_step=0, dbx_step=0,
			du1_step=0, dv1_step=0,
			du2_step=0, dv2_step=0,
			dw1_step=0, dw2_step=0;

		if(dy1) dax_step=dx1/std::fabsf(dy1);
		if(dy2) dbx_step=dx2/std::fabsf(dy2);

		if(dy1) du1_step=du1/std::fabsf(dy1);
		if(dy1) dv1_step=dv1/std::fabsf(dy1);
		if(dy1) dw1_step=dw1/std::fabsf(dy1);
		if(dy2) du2_step=du2/std::fabsf(dy2);
		if(dy2) dv2_step=dv2/std::fabsf(dy2);
		if(dy2) dw2_step=dw2/std::fabsf(dy2);

		//start scanline filling triangles
		if(dy1) {
			for(int j=y1; j<=y2; j++) {
				if(j<ny||j>=my) continue;
				int ax=x1+dax_step*(j-y1);
				int bx=x1+dbx_step*(j-y1);
				float tex_su=u1+du1_step*(j-y1);
				float tex_sv=v1+dv1_step*(j-y1);
				float tex_sw=w1+dw1_step*(j-y1);
				float tex_eu=u1+du2_step*(j-y1);
				float tex_ev=v1+dv2_step*(j-y1);
				float tex_ew=w1+dw2_step*(j-y1);
				//sort along x
				if(ax>bx) {
					std::swap(ax, bx);
					std::swap(tex_su, tex_eu);
					std::swap(tex_sv, tex_ev);
					std::swap(tex_sw, tex_ew);
				}
				t_step=1.f/(bx-ax);
				for(int i=ax; i<bx; i++) {
					if(i<nx||i>=mx) continue;
					t=t_step*(i-ax);
					tex_u=tex_su+t*(tex_eu-tex_su);
					tex_v=tex_sv+t*(tex_ev-tex_sv);
					tex_w=tex_sw+t*(tex_ew-tex_sw);
					float& depth=depth_buffer[bufferIX(i, j)];
					if(tex_w>depth) {
						olc::Pixel col=spr.Sample(tex_u/tex_w, tex_v/tex_w);
						if(col.a!=0) {
							Draw(i, j, tint*col);
							depth=tex_w;
							id_buffer[bufferIX(i, j)]=id;
						}
					}
				}
			}
		}

		//recalculate slopes
		dx1=x3-x2;
		dy1=y3-y2;
		du1=u3-u2;
		dv1=v3-v2;
		dw1=w3-w2;

		if(dy1) dax_step=dx1/std::fabsf(dy1);

		du1_step=0, dv1_step=0;
		if(dy1) du1_step=du1/std::fabsf(dy1);
		if(dy1) dv1_step=dv1/std::fabsf(dy1);
		if(dy1) dw1_step=dw1/std::fabsf(dy1);

		for(int j=y2; j<=y3; j++) {
			if(j<ny||j>=my) continue;
			int ax=x2+dax_step*(j-y2);
			int bx=x1+dbx_step*(j-y1);
			float tex_su=u2+du1_step*(j-y2);
			float tex_sv=v2+dv1_step*(j-y2);
			float tex_sw=w2+dw1_step*(j-y2);
			float tex_eu=u1+du2_step*(j-y1);
			float tex_ev=v1+dv2_step*(j-y1);
			float tex_ew=w1+dw2_step*(j-y1);
			//sort along x
			if(ax>bx) {
				std::swap(ax, bx);
				std::swap(tex_su, tex_eu);
				std::swap(tex_sv, tex_ev);
				std::swap(tex_sw, tex_ew);
			}
			float t_step=1.f/(bx-ax);
			for(int i=ax; i<bx; i++) {
				if(i<nx||i>=mx) continue;
				t=t_step*(i-ax);
				tex_u=tex_su+t*(tex_eu-tex_su);
				tex_v=tex_sv+t*(tex_ev-tex_sv);
				tex_w=tex_sw+t*(tex_ew-tex_sw);
				float& depth=depth_buffer[bufferIX(i, j)];
				if(tex_w>depth) {
					olc::Pixel col=spr.Sample(tex_u/tex_w, tex_v/tex_w);
					if(col.a!=0) {
						Draw(i, j, tint*col);
						depth=tex_w;
						id_buffer[bufferIX(i, j)]=id;
					}
				}
			}
		}
	}
#pragma endregion

	bool user_render() override {
		Clear(olc::Pixel(150, 150, 150));

		//render 3d stuff
		resetBuffers();

		SetPixelMode(olc::Pixel::ALPHA);
		//clear tile bins
		for(int i=0; i<mtr_num_x*mtr_num_y; i++) mtr_bins[i].clear();

		//bin triangles
		for(const auto& tri:tris_to_draw) {
			//compute screen space bounds
			float min_x=std::min(tri.p[0].x, std::min(tri.p[1].x, tri.p[2].x));
			float min_y=std::min(tri.p[0].y, std::min(tri.p[1].y, tri.p[2].y));
			float max_x=std::max(tri.p[0].x, std::max(tri.p[1].x, tri.p[2].x));
			float max_y=std::max(tri.p[0].y, std::max(tri.p[1].y, tri.p[2].y));

			//which tiles does it overlap
			int sx=std::clamp(int(min_x/mtr_tile_size), 0, mtr_num_x-1);
			int sy=std::clamp(int(min_y/mtr_tile_size), 0, mtr_num_y-1);
			int ex=std::clamp(int(max_x/mtr_tile_size), 0, mtr_num_x-1);
			int ey=std::clamp(int(max_y/mtr_tile_size), 0, mtr_num_y-1);

			//add tris to corresponding bins
			for(int tx=sx; tx<=ex; tx++) {
				for(int ty=sy; ty<=ey; ty++) {
					mtr_bins[tx+mtr_num_x*ty].push_back(tri);
				}
			}
		}

		//queue work
		std::atomic<int> tiles_remaining=mtr_jobs.size();
		for(const auto& j:mtr_jobs) {
			mtr_pool->enqueue([&] {
				int nx=std::clamp(mtr_tile_size*j.x, 0, ScreenWidth());
				int ny=std::clamp(mtr_tile_size*j.y, 0, ScreenHeight());
				int mx=std::clamp(mtr_tile_size+nx, 0, ScreenWidth());
				int my=std::clamp(mtr_tile_size+ny, 0, ScreenHeight());
				const auto& bin=mtr_bins[j.x+mtr_num_x*j.y];
				for(const auto& t:bin) {
					int type=0xff&(t.id>>24);
					int spr_ix=0xff&(t.id>>16);
					switch(type) {
						default://meshes
							FillDepthTriangleWithin(
								t.p[0].x, t.p[0].y, t.t[0].w,
								t.p[1].x, t.p[1].y, t.t[1].w,
								t.p[2].x, t.p[2].y, t.t[2].w,
								t.col, t.id,
								nx, ny, mx, my
							);
						case 1:
							//textured cell
							FillTexturedDepthTriangleWithin(
								t.p[0].x, t.p[0].y, t.t[0].u, t.t[0].v, t.t[0].w,
								t.p[1].x, t.p[1].y, t.t[1].u, t.t[1].v, t.t[1].w,
								t.p[2].x, t.p[2].y, t.t[2].u, t.t[2].v, t.t[2].w,
								*texture_atlas[spr_ix], t.col, t.id,
								nx, ny, mx, my
							);
							break;
						case 2:
						{//billboard
							int bb_ix=0xffff&t.id;
							FillTexturedDepthTriangleWithin(
								t.p[0].x, t.p[0].y, t.t[0].u, t.t[0].v, t.t[0].w,
								t.p[1].x, t.p[1].y, t.t[1].u, t.t[1].v, t.t[1].w,
								t.p[2].x, t.p[2].y, t.t[2].u, t.t[2].v, t.t[2].w,
								*texture_atlas[spr_ix], billboards[bb_ix].col, t.id,
								nx, ny, mx, my
							);
							break;
						}
					}
				}
				tiles_remaining--;
			});
		}

		//wait for raster to finish
		while(tiles_remaining);
		SetPixelMode(olc::Pixel::NORMAL);

		for(const auto& l:lines_to_draw) {
			DrawDepthLine(
				l.p[0].x, l.p[0].y, l.t[0].w,
				l.p[1].x, l.p[1].y, l.t[1].w,
				l.col, l.id
			);
		}

		//display messages
		{
			//sort by relative age
			std::sort(messages.begin(), messages.end(), [] (const Message& a, const Message& b) {
				return a.age/a.lifespan<b.age/b.lifespan;
			});

			//display at center from bottom up
			int y=ScreenHeight()-8;
			for(const auto& m:messages) {
				int x=ScreenWidth()/2-4*m.str.length();
				//transparency based on relative age
				float life=m.age/m.lifespan;
				olc::Pixel col=m.col;
				col.a=255*(1-life*life*life);
				DrawString(x, y, m.str, col);
				y-=8;
			}
		}

		{//very basic win/lose screens
			int cx=ScreenWidth()/2, cy=ScreenHeight()/2;
			int scl=11;
			switch(game->state) {
				case Minesweeper::WON: {
					//dark background
					SetPixelMode(olc::Pixel::ALPHA);
					FillRect(0, 0, ScreenWidth(), ScreenHeight(), olc::Pixel(0, 0, 0, 100));
					SetPixelMode(olc::Pixel::NORMAL);
					//green win msg
					olc::Pixel dark_green(0, 180, 0);
					DrawString(cx-8*4*scl, cy-4*scl, "YOU WIN!", dark_green, scl);
					DrawString(cx-22*8, ScreenHeight()-16, "Press R to play again!", dark_green, 2);
					break;
				}
				case Minesweeper::LOST:
					//dark background
					SetPixelMode(olc::Pixel::ALPHA);
					FillRect(0, 0, ScreenWidth(), ScreenHeight(), olc::Pixel(0, 0, 0, 150));
					SetPixelMode(olc::Pixel::NORMAL);
					//red lost msg
					DrawString(cx-9*4*scl, cy-4*scl, "YOU LOSE.", olc::RED, scl);
					DrawString(cx-17*8, ScreenHeight()-16, "Press R to reset.", olc::RED, 2);
					break;
			}
		}

		return true;
	}
};

//i like the pixellated look
int main() {
	Minesweeper3DUI m3dui;
	bool vsync=true;
	if(m3dui.Construct(1000, 800, 1, 1, false, vsync)) m3dui.Start();

	return 0;
}