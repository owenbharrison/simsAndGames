#include "common/3d/engine_3d.h"
using cmn::vf3d;
using cmn::Mat4;

constexpr float Pi=3.1415927f;

struct IndexTriangle {
	int a, b, c;
};

#include "armature.h"

#include "pose.h"

struct VertexData {
	int indexes[4];
	float weights[4];
};

#include "slider.h"

struct RiggingUI : cmn::Engine3D {
	RiggingUI() {
		sAppName="3D Model Rigging";
	}

	//camera positioning
	float cam_yaw=4.15f;
	float cam_pitch=-.42f;
	vf3d light_pos;

	std::vector<vf3d> vertexes;
	std::list<IndexTriangle> index_tris;

	Armature armature;
	std::vector<Pose> poses;

	std::vector<VertexData> vertex_data;

	Slider pose_slider;

	bool user_create() override {
		cam_pos={0.84f, 1.87f, 1.34f};
		light_pos=cam_pos;

		try {
			std::string base_dir="assets/knight";
			
			//load obj file
			std::string model_filename=base_dir+"/model.txt";
			std::ifstream model_file(model_filename);
			if(model_file.fail()) throw std::runtime_error("invalid filename: "+model_filename);

			//parse file line by line
			for(std::string line; std::getline(model_file, line);) {
				std::stringstream line_str(line);
				std::string type; line_str>>type;
				if(type=="v") {
					vf3d v;
					line_str>>v.x>>v.y>>v.z;
					vertexes.push_back(v);
				} else if(type=="f") {
					std::vector<int> v_ixs;

					//parse v/t/n until fail
					for(std::string vtn; line_str>>vtn;) {
						std::stringstream vtn_str(vtn);
						int v_ix;
						if(vtn_str>>v_ix) v_ixs.push_back(v_ix-1);
					}

					//triangulate
					for(int i=2; i<v_ixs.size(); i++) {
						index_tris.push_back({
							v_ixs[0],
							v_ixs[i-1],
							v_ixs[i]
							});
					}
				}
			}

			//load armature
			std::string arm_filename=base_dir+"/bones.arm";
			armature=Armature::loadFromARM(arm_filename);

			//load poses
			for(int i=1; i<=20; i++) {
				std::string pose_filename=base_dir+"/frames/frame"+std::to_string(i)+".pose";
				poses.push_back(Pose::loadFromPOSE(pose_filename));
			}

			//load vertex data
			std::string vertex_data_filename=base_dir+"/vertex_weights.txt";
			std::ifstream vertex_data_file(vertex_data_filename);
			if(vertex_data_file.fail()) throw std::runtime_error("invalid filename: "+vertex_data_filename);
			
			for(std::string line; std::getline(vertex_data_file, line);) {
				std::stringstream line_str(line);
				VertexData vd;
				for(int i=0; i<4; i++) line_str>>vd.indexes[i];
				for(int i=0; i<4; i++) line_str>>vd.weights[i];
				vertex_data.push_back(vd);
			}
		} catch(const std::exception& e) {
			std::cout<<"  "<<e.what()<<'\n';
			return false;
		}

		float margin=30;
		pose_slider={
			olc::vf2d(margin, margin),
			olc::vf2d(ScreenWidth()-margin, margin),
			10
		};

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

		//pose slider input
		const auto slide_action=GetMouse(olc::Mouse::LEFT);
		const auto mouse_pos=GetMousePos();
		if(slide_action.bPressed) pose_slider.startSlide(mouse_pos);
		if(slide_action.bHeld) pose_slider.updateSlide(mouse_pos);
		if(slide_action.bReleased) pose_slider.endSlide();

		return true;
	}

	bool user_geometry() override {
		//add main light
		lights.push_back({light_pos, olc::WHITE});
		
		//get pose from slider
		std::vector<Mat4> mat_pose;
		{
			//get left and right anim
			float ix1f=pose_slider.t*(poses.size()-1);
			int ix1=ix1f;
			int ix2=(1+ix1)%poses.size();

			//should have same size...
			std::vector<Mat4> mat_pose1=armature.calculateAnimationPose(poses[ix1].mat_pose);
			std::vector<Mat4> mat_pose2=armature.calculateAnimationPose(poses[ix2].mat_pose);
			int num=std::min(mat_pose1.size(), mat_pose2.size());
			
			//interpolate
			float t=ix1f-ix1;
			for(int i=0; i<num; i++) {
				const auto& a=mat_pose1[i];
				const auto& b=mat_pose2[i];
				mat_pose.push_back(a+(b-a)*t);
			}
		}
		
		//transform vertexes
		std::vector<vf3d> new_verts(vertexes.size());
		for(int i=0; i<vertexes.size(); i++) {
			const auto& vd=vertex_data[i];
			Mat4 total;
			for(int j=0; j<4; j++) {
				const auto& m=mat_pose[vd.indexes[j]];
				float w=vd.weights[j];
				total=total+m*w;
			}
			new_verts[i]=vertexes[i]*total;
		}

		//tessellate
		for(const auto& it:index_tris) {
			tris_to_project.push_back({new_verts[it.a], new_verts[it.b], new_verts[it.c]});
		}

		return true;
	}

	bool user_render() override {
		Clear(olc::Pixel(60, 60, 60));

		//render 3d stuff
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

		//show slider
		pose_slider.draw(this);

		return true;
	}
};

int main() {
	RiggingUI rui;
	if(rui.Construct(480, 640, 1, 1, false, true)) rui.Start();

	return 0;
}