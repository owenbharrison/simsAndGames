#define OLC_PGE_APPLICATION
#include "olcPixelGameEngine.h"
using olc::vf2d;

#include <vector>

#include <random>

static float random01() {
	static std::mt19937 rng{std::random_device{}()};
	static std::uniform_real_distribution<float> dist(0, 1);
	return dist(rng);
}

#include "vehicle.h"

class SteeringBehaviorsUI : public olc::PixelGameEngine {
	olc::Sprite* prim_circ_spr=nullptr;
	olc::Decal* prim_circ_dec=nullptr;
	olc::Sprite* prim_rect_spr=nullptr;
	olc::Decal* prim_rect_dec=nullptr;

	olc::vi2d letter_sizes[62];
	std::vector<vf2d> letter_points[62];

	std::vector<Vehicle> vehicles;

public:
	SteeringBehaviorsUI() {
		sAppName="Steering Behaviors";
	}

	bool OnUserCreate() override {
		//make some "primitives" to draw with
		prim_rect_spr=new olc::Sprite(1, 1);
		prim_rect_spr->SetPixel(0, 0, olc::WHITE);
		prim_rect_dec=new olc::Decal(prim_rect_spr);
		{
			int sz=1024;
			prim_circ_spr=new olc::Sprite(sz, sz);
			SetDrawTarget(prim_circ_spr);
			Clear(olc::BLANK);
			FillCircle(sz/2, sz/2, sz/2);
			SetDrawTarget(nullptr);
			prim_circ_dec=new olc::Decal(prim_circ_spr);
		}

		//for each letter
		for(int l=0; l<62; l++) {
			//load letter sprite
			std::string filename="assets/";
			char c;
			if(l<26) filename+="lower/", c='a'+l;
			else if(l<52) filename+="upper/", c='A'+(l-26);
			else filename+="digit/", c='0'+(l-52);
			filename+=std::string(1, c)+".png";
			olc::Sprite* spr=new olc::Sprite(filename);

			//store sprite size
			letter_sizes[l]={spr->width, spr->height};

			//for each pixel
			for(int i=0; i<spr->width-1; i++) {
				for(int j=0; j<spr->height-1; j++) {
					const auto& curr=spr->GetPixel(i, j);

					//check right
					const auto& right=spr->GetPixel(1+i, j);
					int cr_dr=curr.r-right.r;
					int cr_dg=curr.g-right.g;
					int cr_db=curr.b-right.b;
					int cr_dist_sq=cr_dr*cr_dr+cr_dg*cr_dg+cr_db*cr_db;

					//check down
					const auto& down=spr->GetPixel(i, 1+j);
					int cd_dr=curr.r-down.r;
					int cd_dg=curr.g-down.g;
					int cd_db=curr.b-down.b;
					int cd_dist_sq=cd_dr*cd_dr+cd_dg*cd_dg+cd_db*cd_db;

					//if different: likely an edge
					if(cr_dist_sq>35*35||cd_dist_sq>35*35) {
						//use uv coords
						vf2d uv((.5f+i)/spr->width, (.5f+j)/spr->height);

						//only place new point so close to any other
						bool unique=true;
						for(const auto& p:letter_points[l]) {
							if((uv-p).mag2()<.0025f) {
								unique=false;
								break;
							}
						}
						if(unique) letter_points[l].push_back(uv);
					}
				}
			}
			delete spr;
		}

		//place some text in middle of screen
		{
			std::string str="testing\nABC123\nqwerty";
			auto height=120;
			vf2d size=textSize(str, height);
			vf2d ctr(ScreenWidth()/2, ScreenHeight()/2);
			const auto positions=textToDots(ctr-size/2, str, height);
		
			//vehicle target should be points on text contour
			for(const auto& t:positions) {
				//random start position
				vf2d pos(
					ScreenWidth()*random01(),
					ScreenHeight()*random01()
				);
				//random BRIGHT color
				float r=random01(), g=random01(), b=random01();
				float l=std::max(r, std::max(g, b));
				olc::Pixel col=olc::PixelF(r/l, g/l, b/l);
				vehicles.push_back(Vehicle(pos, t, col));
			}
		}

		return true;
	}

	bool OnUserDestroy() override {
		delete prim_rect_dec;
		delete prim_rect_spr;
		delete prim_circ_dec;
		delete prim_circ_spr;

		return true;
	}

	//find bounds of string
	vf2d textSize(const std::string& str, float height) {
		//for every character in string
		vf2d offset, size;
		for(const auto& c:str) {
			int ix=-1;

			//space spacing :D
			if(c==' ') offset.x+=.5f*height;
			//newline formatting
			else if(c=='\n') offset.x=0, offset.y+=height;

			else if(c>='a'&&c<='z') ix=c-'a';
			else if(c>='A'&&c<='Z') ix=26+c-'A';
			else if(c>='0'&&c<='9') ix=52+c-'0';

			if(ix==-1) continue;

			//letter spacing...

			//aspect ratio to find scaled letter
			const auto& l_sz=letter_sizes[ix];
			float width=height*l_sz.x/l_sz.y;
			size.x=std::max(size.x, offset.x+width);
			size.y=std::max(size.y, offset.y+height);

			offset.x+=width;
		}
		return size;
	}
	
	std::vector<vf2d> textToDots(const vf2d& pos, const std::string& str, float height) {
		//for every character in string
		vf2d offset=pos;
		std::vector<vf2d> pts;
		for(const auto& c:str) {
			int ix=-1;

			//space spacing :D
			if(c==' ') offset.x+=.5f*height;
			//newline formatting
			else if(c=='\n') offset.x=pos.x, offset.y+=height;

			else if(c>='a'&&c<='z') ix=c-'a';
			else if(c>='A'&&c<='Z') ix=26+c-'A';
			else if(c>='0'&&c<='9') ix=52+c-'0';
			
			if(ix==-1) continue;
			
			//letter spacing...
			
			//aspect ratio to find scaled letter
			const auto& sz=letter_sizes[ix];
			vf2d scaled(height*sz.x/sz.y, height);
			for(const auto& uv:letter_points[ix]) {
				pts.push_back(offset+scaled*uv);
			}
			offset.x+=scaled.x;
		}
		return pts;
	}

	void DrawThickLineDecal(const vf2d& a, const vf2d& b, float w, olc::Pixel col) {
		vf2d sub=b-a;
		float len=sub.mag();
		vf2d tang=(sub/len).perp();

		float angle=std::atan2(sub.y, sub.x);
		DrawRotatedDecal(a-w*tang, prim_rect_dec, angle, {0, 0}, {len, 2*w}, col);
	}

	void FillCircleDecal(const vf2d& pos, float rad, const olc::Pixel& col) {
		vf2d offset(rad, rad);
		vf2d scale{2*rad/prim_circ_spr->width, 2*rad/prim_circ_spr->width};
		DrawDecal(pos-offset, prim_circ_dec, scale, col);
	}

	bool OnUserUpdate(float dt) override {
		const vf2d mouse_pos=GetMousePos();

		//steering behaviors
		for(auto& v:vehicles) {
			//arrive at target
			v.accelerate(20*v.getArrive(v.target));

			//if mouse too close, flee
			if((mouse_pos-v.pos).mag2()<1600) {
				v.accelerate(50*v.getFlee(mouse_pos));
			}

			//kinematics
			v.update(dt);
		}

		//draw background grid
		FillRectDecal({0, 0}, GetScreenSize(), olc::BLACK);

		//show grid
		{
			const auto grid_spacing=30;

			const int num_x=1+ScreenWidth()/grid_spacing;
			const int num_y=1+ScreenHeight()/grid_spacing;

			//vertical lines
			for(int i=0; i<=num_x; i++) {
				float x=grid_spacing*i;
				vf2d top(x, 0), btm(x, ScreenHeight());
				if(i%5==0) DrawThickLineDecal(top, btm, 2, olc::VERY_DARK_GREY);
				else DrawLineDecal(top, btm, olc::VERY_DARK_GREY);
			}

			//horizontal lines
			for(int j=0; j<=num_y; j++) {
				float y=grid_spacing*j;
				vf2d lft(0, y), rgt(ScreenWidth(), y);
				if(j%5==0) DrawThickLineDecal(lft, rgt, 2, olc::VERY_DARK_GREY);
				else DrawLineDecal(lft, rgt, olc::VERY_DARK_GREY);
			}
		}

		//draw "vehicles"
		for(const auto& v:vehicles) {
			FillCircleDecal(v.pos, 3.2f, v.col);
		}

		return true;
	}
};

int main() {
	SteeringBehaviorsUI sbui;
	bool vsync=true;
	if(sbui.Construct(720, 480, 1, 1, false, vsync)) sbui.Start();

	return 0;
}