/*====="SHADOW" SHADER=====*/

@vs vs_shadow

layout(binding=0) uniform vs_shadow_params {
	mat4 u_model;
	mat4 u_mvp;
};

in vec3 i_pos;
in vec3 i_norm;
in vec2 i_uv;

out vec3 norm;

void main() {
	norm=normalize(mat3(u_model)*i_norm);
	gl_Position=u_mvp*vec4(i_pos, 1);
}

@end

@fs fs_shadow

in vec3 norm;

out vec4 o_frag_col;

void main() {
	o_frag_col=vec4(.5+.5*norm, 1);
}

@end

@program shadow vs_shadow fs_shadow





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





/*=====LINE SHADER=====*/

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

in vec4 col;

out vec4 o_frag_col;

void main() {
	o_frag_col=col;
}

@end

@program line vs_line fs_line





/*=====DEBUG TEXTURE VIEW SHADER=====*/

@vs vs_texview

layout(binding=0) uniform vs_texview_params {
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

@fs fs_texview

layout(binding=0) uniform texture2D u_texview_tex;
layout(binding=0) uniform sampler u_texview_smp;

in vec2 uv;

out vec4 o_frag_col;

void main() {
	o_frag_col=texture(sampler2D(u_texview_tex, u_texview_smp), uv);
}

@end

@program texview vs_texview fs_texview