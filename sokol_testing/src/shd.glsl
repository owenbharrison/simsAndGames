@block vs_uniforms
layout(binding=0) uniform vs_params {
	mat4 u_mvp;
}; 
@end

@vs vs_skybox

@include_block vs_uniforms

in vec3 v_pos;
in vec3 v_norm;
in vec2 v_uv;

out vec2 uv;

void main() {
	//z component=depth=1
	vec4 pos=u_mvp*vec4(v_pos, 1);
	gl_Position=pos.xyww;
	uv=v_uv;
}
@end

@fs fs_skybox

layout(binding=0) uniform texture2D skybox_tex;
layout(binding=0) uniform sampler skybox_smp;

in vec2 uv;

out vec4 frag_color;

void main() {
	frag_color=texture(sampler2D(skybox_tex, skybox_smp), uv);
}
@end

@program skybox vs_skybox fs_skybox

@vs vs_default

@include_block vs_uniforms

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

layout(binding=0) uniform fs_params{
	vec3 u_eye_pos;
};

layout(binding=0) uniform texture2D default_tex;
layout(binding=1) uniform texture2D default_px;
layout(binding=2) uniform texture2D default_nx;
layout(binding=3) uniform texture2D default_py;
layout(binding=4) uniform texture2D default_ny;
layout(binding=5) uniform texture2D default_pz;
layout(binding=6) uniform texture2D default_nz;
layout(binding=0) uniform sampler default_smp;

in vec2 uv;

out vec4 frag_color;

void main() {
	
	frag_color=texture(sampler2D(default_tex, default_smp), uv);
}
@end

@program default vs_default fs_default