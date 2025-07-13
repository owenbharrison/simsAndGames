#pragma once
#ifndef ANIMATED_MESH_STRUCT_H
#define ANIMATED_MESH_STRUCT_H

#include "armature.h"

#include "pose.h"

struct VertexData {
	int indexes[4];
	float weights[4];
};

struct AnimatedMesh {
	std::vector<vf3d> vertexes;
	std::list<IndexTriangle> index_tris;
	
	vf3d rotation, scale{1, 1, 1}, translation;
	Mat4 mat_world;

	Armature armature;
	std::vector<Pose> poses;
	std::vector<VertexData> vertex_data;

	std::vector<cmn::Triangle> tris;

	void updateTransforms() {
		//combine all transforms
		Mat4 mat_rot_x=Mat4::makeRotX(rotation.x);
		Mat4 mat_rot_y=Mat4::makeRotY(rotation.y);
		Mat4 mat_rot_z=Mat4::makeRotZ(rotation.z);
		Mat4 mat_rot=mat_rot_x*mat_rot_y*mat_rot_z;
		Mat4 mat_scale=Mat4::makeScale(scale.x, scale.y, scale.z);
		Mat4 mat_trans=Mat4::makeTrans(translation.x, translation.y, translation.z);
		mat_world=mat_scale*mat_rot*mat_trans;
	}

	void applyAnimation(float t) {
		//get pose from t
		std::vector<Mat4> mat_pose;
		{
			//get left and right anim
			float ix1f=t*(poses.size()-1);
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
			Mat4 mat_anim;
			for(int j=0; j<4; j++) {
				const auto& m=mat_pose[vd.indexes[j]];
				float w=vd.weights[j];
				mat_anim=mat_anim+m*w;
			}
			Mat4 mat_total=mat_anim*mat_world;
			new_verts[i]=vertexes[i]*mat_total;
		}

		//tessellate
		tris.clear();
		for(const auto& it:index_tris) {
			tris.push_back({new_verts[it.a], new_verts[it.b], new_verts[it.c]});
		}
	}

	static AnimatedMesh load(const std::string& base_dir, int start, int end) {
		//load obj file
		std::string model_filename=base_dir+"/model.txt";
		std::ifstream model_file(model_filename);
		if(model_file.fail()) throw std::runtime_error("invalid filename: "+model_filename);

		AnimatedMesh anim_mesh;

		//parse file line by line
		for(std::string line; std::getline(model_file, line);) {
			std::stringstream line_str(line);
			std::string type; line_str>>type;
			if(type=="v") {
				vf3d v;
				line_str>>v.x>>v.y>>v.z;
				anim_mesh.vertexes.push_back(v);
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
					anim_mesh.index_tris.push_back({
						v_ixs[0],
						v_ixs[i-1],
						v_ixs[i]
						});
				}
			}
		}

		//load armature
		std::string arm_filename=base_dir+"/bones.arm";
		anim_mesh.armature=Armature::loadFromARM(arm_filename);

		//load poses
		for(int i=start; i<=end; i++) {
			std::string pose_filename=base_dir+"/frames/frame"+std::to_string(i)+".pose";
			anim_mesh.poses.push_back(Pose::loadFromPOSE(pose_filename));
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
			anim_mesh.vertex_data.push_back(vd);
		}

		return anim_mesh;
	}
};
#endif