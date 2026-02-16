/*=====SKYBOX SHADER=====*/

@vs vs_skybox

in vec3 v_pos;
in vec2 v_uv;

out vec2 uv;

layout(binding=0) uniform vs_skybox_params {
	mat4 u_mvp;
};

void main() {
	//z component=depth=1
	vec4 pos=u_mvp*vec4(v_pos, 1);
	gl_Position=pos.xyww;
	uv=v_uv;
}

@end

@fs fs_skybox

in vec2 uv;

out vec4 frag_color;

layout(binding=0) uniform texture2D skybox_tex;
layout(binding=0) uniform sampler skybox_smp;

void main() {
	frag_color=texture(sampler2D(skybox_tex, skybox_smp), uv);
}

@end

@program skybox vs_skybox fs_skybox





/*=====CUBE SHADER=====*/

@vs vs_cube

in vec3 v_pos;
in vec3 v_norm;

out vec3 pos;
out vec3 norm;

layout(binding=0) uniform vs_cube_params {
	mat4 u_model;
	mat4 u_mvp;
};

void main() {
	gl_Position=u_mvp*vec4(v_pos, 1);

	pos=(u_model*vec4(v_pos, 1)).xyz;

	norm=normalize(mat3(u_model)*v_norm);
}

@end

@fs fs_cube

in vec3 pos;
in vec3 norm;

out vec4 frag_col;

layout(binding=1) uniform fs_cube_params {
	vec3 u_col;
	vec3 u_light_pos;
	vec3 u_eye_pos;
};

void main() {
	vec3 N=normalize(norm);
	vec3 L=normalize(u_light_pos-pos);
	vec3 V=normalize(u_eye_pos-pos);
	vec3 R=reflect(-L, N);
	
	float amb_mag=.3;
	float diff_mag=.7*max(dot(N, L), 0);

	//white specular
	float spec_mag=.3*pow(max(dot(R, V), 0), 32);
	vec3 spec=spec_mag*vec3(1, 1, 1);

	frag_col=vec4(u_col*(amb_mag+diff_mag)+spec, 1);
}

@end

@program cube vs_cube fs_cube





/*==== COLORVIEW SHADER ====*/

@vs vs_colorview

in vec2 i_pos;
in vec2 i_uv;

out vec2 uv;

layout(binding=0) uniform vs_colorview_params {
	vec2 u_tl;
	vec2 u_br;
};

void main() {
	uv=u_tl+i_uv*(u_br-u_tl);
	gl_Position=vec4(i_pos, .5, 1);
}

@end

@fs fs_colorview

in vec2 uv;

out vec4 o_frag_col;

layout(binding=0) uniform texture2D u_colorview_tex;
layout(binding=0) uniform sampler u_colorview_smp;

layout(binding=1) uniform fs_colorview_params {
	vec4 u_tint;
};

void main() {
	vec4 col=texture(sampler2D(u_colorview_tex, u_colorview_smp), uv);
	o_frag_col=u_tint*col;
}

@end

@program colorview vs_colorview fs_colorview