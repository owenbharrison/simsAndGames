//no projection needed...
@vs vs_skybox

layout(binding=0) uniform skybox_vs_params {
	mat4 u_view_proj;
}; 

in vec3 v_pos;

out vec3 dir;

void main() {
	dir=v_pos;
	vec4 pos=u_view_proj*vec4(v_pos, 1);

	//z component=depth=1
	gl_Position=pos.xyww;
}
@end

@fs fs_skybox

layout(binding=0) uniform texture2D skybox_px;
layout(binding=1) uniform texture2D skybox_nx;
layout(binding=2) uniform texture2D skybox_py;
layout(binding=3) uniform texture2D skybox_ny;
layout(binding=4) uniform texture2D skybox_pz;
layout(binding=5) uniform texture2D skybox_nz;
layout(binding=0) uniform sampler skybox_smp;

in vec3 dir;

const float Pi=3.1415927;

out vec4 frag_color;

void main() {
	vec3 absdir=abs(dir);
	bool xy=absdir.x>absdir.y;
	bool yz=absdir.y>absdir.z;
	bool zx=absdir.z>absdir.x;
	if(xy&&!zx) {
		float u=.5+2/Pi*atan(dir.z/dir.x);
		float v=.5-2/Pi*atan(dir.y/dir.x);
		if(dir.x>0) frag_color=texture(sampler2D(skybox_px, skybox_smp), vec2(u, v));
		else frag_color=texture(sampler2D(skybox_nx, skybox_smp), vec2(u, 1-v));
	}
	if(yz&&!xy) {
		float u=.5-2/Pi*atan(dir.x/dir.y);
		float v=.5+2/Pi*atan(dir.z/dir.y);
		if(dir.y>0) frag_color=texture(sampler2D(skybox_py, skybox_smp), vec2(u, v));
		else frag_color=texture(sampler2D(skybox_ny, skybox_smp), vec2(1-u, v));
	}
	if(zx&&!yz) {
		float u=.5-2/Pi*atan(dir.x/dir.z);
		float v=.5-2/Pi*atan(dir.y/dir.z);
		if(dir.z>0) frag_color=texture(sampler2D(skybox_pz, skybox_smp), vec2(u, v));
		else frag_color=texture(sampler2D(skybox_nz, skybox_smp), vec2(u, 1-v));
	}
}
@end

@program skybox vs_skybox fs_skybox

@vs vs_default

layout(binding=0) uniform default_vs_params {
	mat4 u_mvp;
};

in vec3 v_pos;
in vec3 v_norm;
in vec2 v_uv;

out vec2 uv;

void main() {
	gl_Position=u_mvp*vec4(v_pos, 1);
	uv=v_uv;
}
@end

@fs fs_default

layout(binding=0) uniform texture2D default_tex;
layout(binding=0) uniform sampler default_smp;

in vec2 uv;

out vec4 frag_color;

void main() {
	frag_color=texture(sampler2D(default_tex, default_smp), uv);
}
@end

@program default vs_default fs_default