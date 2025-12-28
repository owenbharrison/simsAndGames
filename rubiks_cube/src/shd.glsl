/*=====SKYBOX SHADER=====*/

@vs vs_skybox

layout(binding=0) uniform vs_skybox_params {
	mat4 u_mvp;
};

in vec3 v_pos;
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





/*=====CUBE SHADER=====*/

@vs vs_cube

layout(binding=0) uniform vs_cube_params {
	mat4 u_mvp;
};

in vec3 v_pos;

void main() {
	gl_Position=u_mvp*vec4(v_pos, 1);
}

@end

@fs fs_cube

layout(binding=1) uniform fs_cube_params {
	vec4 u_col;
};

out vec4 frag_color;

void main() {
	frag_color=u_col;
}

@end

@program cube vs_cube fs_cube
