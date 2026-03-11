/*=====SKYBOX SHADER=====*/

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





/*=====TERRAIN SHADER=====*/

@vs vs_terrain

in vec3 i_pos;
in vec3 i_norm;
in vec2 i_uv;

out vec3 pos;
out vec3 norm;

layout(binding=0) uniform p_vs_terrain {
	mat4 u_model;
	mat4 u_mvp;
};

void main() {
	pos=(u_model*vec4(i_pos, 1)).xyz;
	norm=normalize(mat3(u_model)*i_norm);
	gl_Position=u_mvp*vec4(i_pos, 1);
}

@end

@fs fs_terrain

in vec3 pos;
in vec3 norm;

out vec4 o_frag_col;

layout(binding=1) uniform p_fs_terrain {
	vec3 u_eye_pos;
	vec3 u_light_dir;
};

layout(binding=0) uniform texture2D b_terrain_tex;
layout(binding=0) uniform sampler b_terrain_smp;

void main() {
	vec3 N=normalize(norm);
	vec3 L=normalize(u_light_dir);
	vec3 V=normalize(u_eye_pos-pos);
	vec3 R=reflect(-L, N);

	float amb_mag=.2;
	float diff_mag=.7*max(dot(L, N), 0);
	float spec_mag=.3*pow(max(dot(R, V), 0), 32);
	
	//base texture color
	vec3 up=vec3(0, 1, 0);
	float theta=acos(abs(dot(up, N)));
	float theta01=theta/(.5*3.1415927);
	vec4 col=texture(sampler2D(b_terrain_tex, b_terrain_smp), vec2(theta01, 0));
	//white specular
	vec3 spec=spec_mag*vec3(1, 1, 1);

	o_frag_col=vec4(spec+col.rgb*(amb_mag+diff_mag), 1);
}

@end

@program terrain vs_terrain fs_terrain





/*=====COLOR VIEW SHADER=====*/

@vs vs_colorview

in vec2 i_pos;
in vec2 i_uv;

out vec2 uv;

layout(binding=0) uniform p_vs_colorview {
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

layout(binding=1) uniform p_fs_colorview {
	vec4 u_tint;
};

layout(binding=0) uniform texture2D b_colorview_tex;
layout(binding=0) uniform sampler b_colorview_smp;

void main() {
	vec4 col=texture(sampler2D(b_colorview_tex, b_colorview_smp), uv);
	o_frag_col=u_tint*col;
}

@end

@program colorview vs_colorview fs_colorview