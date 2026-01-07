/*=====SKYBOX SHADER=====*/

@vs vs_skybox

layout(binding=0) uniform vs_skybox_params {
	mat4 u_mvp;
};

in vec3 i_pos;
in vec2 i_uv;

out vec2 uv;

void main() {
	//z component=depth=1
	vec4 pos=u_mvp*vec4(i_pos, 1);
	gl_Position=pos.xyww;
	uv=i_uv;
}

@end

@fs fs_skybox

layout(binding=0) uniform texture2D u_skybox_tex;
layout(binding=0) uniform sampler u_skybox_smp;

in vec2 uv;

out vec4 o_frag_col;

void main() {
	o_frag_col=texture(sampler2D(u_skybox_tex, u_skybox_smp), uv);
}

@end

@program skybox vs_skybox fs_skybox





/*=====TERRAIN SHADER=====*/

@vs vs_terrain

layout(binding=0) uniform vs_terrain_params {
	mat4 u_model;
	mat4 u_mvp;
};

in vec3 i_pos;
in vec3 i_norm;
in vec2 i_uv;

out vec3 pos;
out vec3 norm;

void main() {
	pos=(u_model*vec4(i_pos, 1)).xyz;
	norm=normalize(mat3(u_model)*i_norm);
	gl_Position=u_mvp*vec4(i_pos, 1);
}

@end

@fs fs_terrain

layout(binding=1) uniform fs_terrain_params {
	vec3 u_eye_pos;
	vec3 u_light_pos;
};
layout(binding=0) uniform texture2D u_terrain_tex;
layout(binding=0) uniform sampler u_terrain_smp;

in vec3 pos;
in vec3 norm;

out vec4 o_frag_col;

void main() {
	vec3 N=normalize(norm);
	vec3 L=normalize(u_light_pos-pos);
	vec3 V=normalize(u_eye_pos-pos);
	vec3 R=reflect(-L, N);

	float amb_mag=.2;
	float diff_mag=.7*max(dot(N, L), 0);

	//white specular
	float spec_mag=.3*pow(max(dot(R, V), 0), 32);
	vec3 spec=spec_mag*vec3(1, 1, 1);

	//sample gradient with steepness
	vec3 up=vec3(0, 1, 0);
	//map [0, pi/2] to [0, 1]
	float u=2/3.1415927*acos(abs(dot(up, N)));
	vec4 col=texture(sampler2D(u_terrain_tex, u_terrain_smp), vec2(u, 0));
	o_frag_col=vec4((amb_mag+diff_mag)*col.rgb+spec, 1);
}

@end

@program terrain vs_terrain fs_terrain





/*=====LINE SHADER=====*/

@vs vs_line

layout(binding=0) uniform vs_line_params {
	mat4 u_mvp;
};

in vec3 i_pos;
in vec4 i_col;

out vec4 col;

void main() {
	gl_Position=u_mvp*vec4(i_pos, 1);
	col=i_col;
}

@end

@fs fs_line

in vec4 col;

out vec4 o_frag_col;

void main() {
	o_frag_col=col;
}

@end

@program line vs_line fs_line