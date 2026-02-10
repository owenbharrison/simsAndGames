/*=====SHADOW MAP SHADER=====*/

@vs vs_shadow_map

in vec3 i_pos;
in vec3 i_norm;
in vec2 i_uv;

out vec3 pos;

layout(binding=0) uniform vs_shadow_map_params {
	mat4 u_model;
	mat4 u_mvp;
};

void main() {
	pos=(u_model*vec4(i_pos, 1)).xyz;
	gl_Position=u_mvp*vec4(i_pos, 1);
}

@end

@fs fs_shadow_map

in vec3 pos;

out vec4 o_frag_col;

layout(binding=1) uniform fs_shadow_map_params {
	vec3 u_light_pos;
	float u_cam_far;
};

//float->int
vec4 encodeFloat(float f) {
	vec4 enc=fract(f*vec4(1., 255., 65025., 16581375.));
	return enc-enc.yzwx*vec4(1./255., 1./255., 1./255., 0.);
}

void main() {
	float dist=length(u_light_pos-pos);
	float dist01=dist/u_cam_far;
	o_frag_col=encodeFloat(dist01);
}

@end

@program shadow_map vs_shadow_map fs_shadow_map





/*=====SKYBOX SHADER=====*/

@vs vs_skybox

in vec3 i_pos;
in vec2 i_uv;

out vec2 uv;

layout(binding=0) uniform vs_skybox_params {
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

layout(binding=0) uniform texture2D u_skybox_tex;
layout(binding=0) uniform sampler u_skybox_smp;

void main() {
	o_frag_col=texture(sampler2D(u_skybox_tex, u_skybox_smp), uv);
}

@end

@program skybox vs_skybox fs_skybox





/*=====MESH SHADER=====*/

@vs vs_mesh

in vec3 i_pos;
in vec3 i_norm;
in vec2 i_uv;

out vec3 pos;
out vec3 norm;
out vec2 uv;

layout(binding=0) uniform vs_mesh_params {
	mat4 u_model;
	mat4 u_mvp;
};

void main() {
	pos=(u_model*vec4(i_pos, 1)).xyz;
	norm=normalize(mat3(u_model)*i_norm);
	uv=i_uv;
	gl_Position=u_mvp*vec4(i_pos, 1);
}

@end

@fs fs_mesh

in vec3 pos;
in vec3 norm;
in vec2 uv;

out vec4 o_frag_col;

layout(binding=1) uniform fs_mesh_params {
	vec3 u_eye_pos;
	vec3 u_light_pos;
	float u_cam_far;
};

layout(binding=0) uniform texture2D u_mesh_tex;
layout(binding=0) uniform sampler u_mesh_smp;

layout(binding=1) uniform textureCube u_mesh_shadow_tex;
layout(binding=1) uniform sampler u_mesh_shadow_smp;

//int->float
float decodeFloat(vec4 enc) {
	return dot(enc, 1./vec4(1., 255., 65025., 16581375.));
}

void main() {
	vec3 N=normalize(norm);
	vec3 L=normalize(u_light_pos-pos);
	vec3 V=normalize(u_eye_pos-pos);
	vec3 R=reflect(-L, N);

	float amb_mag=.2;
	float diff_mag=.7*max(dot(L, N), 0);
	float spec_mag=.3*pow(max(dot(R, V), 0), 32);

	//is in shadow?
	vec4 rgba=texture(samplerCube(u_mesh_shadow_tex, u_mesh_shadow_smp), -L);
	float dist01=decodeFloat(rgba);
	float dist=dist01*u_cam_far;
	if(length(u_light_pos-pos)>dist+.05) diff_mag=0, spec_mag=0;
	
	//base texture color
	vec4 col=texture(sampler2D(u_mesh_tex, u_mesh_smp), uv);
	//white specular
	vec3 spec=spec_mag*vec3(1, 1, 1);

	o_frag_col=vec4(spec+col.rgb*(amb_mag+diff_mag), 1);
}

@end

@program mesh vs_mesh fs_mesh





/*=====LINE SHADER=====*/

@vs vs_line

in vec3 i_pos;
in vec4 i_col;

out vec4 col;

layout(binding=0) uniform vs_line_params {
	mat4 u_mvp;
};

void main() {
	col=i_col;
	gl_Position=u_mvp*vec4(i_pos, 1);
}

@end

@fs fs_line

in vec4 col;

out vec4 o_frag_col;

layout(binding=1) uniform fs_line_params {
	vec4 u_tint;
};

void main() {
	o_frag_col=u_tint*col;
}

@end

@program line vs_line fs_line





/*=====COLOR VIEW SHADER=====*/

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

layout(binding=1) uniform fs_colorview_params {
	vec4 u_tint;
};

layout(binding=0) uniform texture2D u_colorview_tex;
layout(binding=0) uniform sampler u_colorview_smp;

void main() {
	vec4 col=texture(sampler2D(u_colorview_tex, u_colorview_smp), uv);
	o_frag_col=u_tint*col;
}

@end

@program colorview vs_colorview fs_colorview





/*=====BASECOL SHADER=====*/

@vs vs_basecol

in vec3 i_pos;

layout(binding=0) uniform vs_basecol_params {
	mat4 u_mvp;
};

void main() {
	gl_Position=u_mvp*vec4(i_pos, 1);
}

@end

@fs fs_basecol

out vec4 o_frag_col;

layout(binding=1) uniform fs_basecol_params {
	vec4 u_tint;
};

void main() {
	o_frag_col=u_tint;
}

@end

@program basecol vs_basecol fs_basecol