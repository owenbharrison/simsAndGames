/*=====SKYBOX SHADER=====*/

@vs vs_skybox

layout(binding=0) uniform vs_skybox_params {
	mat4 u_mvp;
};

in vec3 i_pos;
in vec2 i_uv;

out vec2 uv;

void main() {
	uv=i_uv;
	//z component=depth=1
	vec4 pos=u_mvp*vec4(i_pos, 1);
	gl_Position=pos.xyww;
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





/*=====MESH SHADER=====*/

@vs vs_mesh

layout(binding=0) uniform vs_mesh_params {
	mat4 u_model;
	mat4 u_mvp;
};

in vec3 i_pos;
in vec3 i_norm;
in vec2 i_uv;

out vec3 pos;
out vec3 norm;
out vec2 uv;

void main() {
	pos=(u_model*vec4(i_pos, 1)).xyz;
	norm=normalize(mat3(u_model)*i_norm);
	uv=i_uv;
	gl_Position=u_mvp*vec4(i_pos, 1);
}

@end

@fs fs_mesh

layout(binding=1) uniform fs_mesh_params {
	vec3 u_eye_pos;
	vec3 u_light_pos;
};

layout(binding=0) uniform texture2D u_mesh_tex;
layout(binding=0) uniform sampler u_mesh_smp;

in vec3 pos;
in vec3 norm;
in vec2 uv;

out vec4 o_frag_col;

void main() {
	vec3 N=normalize(norm);
	vec3 L=normalize(u_light_pos-pos);
	vec3 V=normalize(u_eye_pos-pos);
	vec3 R=reflect(-L, N);

	float amb_mag=.2;
	float diff_mag=.7*max(dot(N, L), 0);
	vec4 col=texture(sampler2D(u_mesh_tex, u_mesh_smp), uv);

	//white specular
	float spec_mag=.3*pow(max(dot(R, V), 0), 32);
	vec3 spec=spec_mag*vec3(1, 1, 1);

	o_frag_col=vec4(col.rgb*(amb_mag+diff_mag)+spec, 1);
}

@end

@program mesh vs_mesh fs_mesh





/*=====line SHADER=====*/

@vs vs_line

layout(binding=0) uniform vs_line_params {
	mat4 u_mvp;
};

in vec3 i_pos;
in vec4 i_col;

out vec4 col;

void main() {
	col=i_col;
	gl_Position=u_mvp*vec4(i_pos, 1);
}

@end

@fs fs_line

layout(binding=1) uniform fs_line_params {
	vec4 u_tint;
};

in vec4 col;

out vec4 o_frag_col;

void main() {
	o_frag_col=u_tint*col;
}

@end

@program line vs_line fs_line





/*=====COLOR VIEW SHADER=====*/

@vs vs_colorview

layout(binding=0) uniform vs_colorview_params {
	vec2 u_tl;
	vec2 u_br;
};

in vec2 i_pos;
in vec2 i_uv;

out vec2 uv;

void main() {
	uv=u_tl+i_uv*(u_br-u_tl);
	gl_Position=vec4(i_pos, .5, 1);
}

@end

@fs fs_colorview

layout(binding=1) uniform fs_colorview_params {
	vec4 u_tint;
};

layout(binding=0) uniform texture2D u_colorview_tex;
layout(binding=0) uniform sampler u_colorview_smp;

in vec2 uv;

out vec4 o_frag_col;

void main() {
	vec4 col=texture(sampler2D(u_colorview_tex, u_colorview_smp), uv);
	o_frag_col=u_tint*col;
}

@end

@program colorview vs_colorview fs_colorview





/*=====BASECOL SHADER=====*/

@vs vs_basecol

layout(binding=0) uniform vs_basecol_params {
	mat4 u_mvp;
};

in vec3 i_pos;

void main() {
	gl_Position=u_mvp*vec4(i_pos, 1);
}

@end

@fs fs_basecol

layout(binding=1) uniform fs_basecol_params {
	vec4 u_tint;
};

out vec4 o_frag_col;

void main() {
	o_frag_col=u_tint;
}

@end

@program basecol vs_basecol fs_basecol