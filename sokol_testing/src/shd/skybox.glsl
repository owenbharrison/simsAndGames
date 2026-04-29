//depth-avoided textured 3d shader

@vs vs_skybox

in vec3 i_pos;
in vec2 i_uv;

out vec2 uv;

layout(binding=0) uniform p_vs_skybox {
	mat4 u_mvp;
};

void main() {
	uv=i_uv;
	//z component=depth=1
	vec4 pos=u_mvp*vec4(i_pos, 1);
	gl_Position=pos.xyww;
}

@end

@fs fs_skybox

in vec2 uv;

out vec4 o_frag_col;

layout(binding=0) uniform texture2D b_skybox_tex;
layout(binding=0) uniform sampler b_skybox_smp;

void main() {
	o_frag_col=texture(sampler2D(b_skybox_tex, b_skybox_smp), uv);
}

@end

@program skybox vs_skybox fs_skybox