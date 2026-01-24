/*====="SHADOW" SHADER=====*/

@vs vs_shadow
@glsl_options fixup_clipspace

layout(binding=0) uniform vs_shadow_params {
	mat4 u_mvp;
};

in vec3 i_pos;
in vec3 i_col;

void main() {
	gl_Position=u_mvp*vec4(i_pos, 1);
}

@end

@fs fs_shadow

void main() {}

@end

@program shadow vs_shadow fs_shadow





/*=====CUBE SHADER=====*/

@vs vs_cube

layout(binding=1) uniform vs_cube_params {
	mat4 u_mvp;
};

in vec3 i_pos;
in vec3 i_col;

out vec3 col;

void main() {
	col=i_col;
	gl_Position=u_mvp*vec4(i_pos, 1);
}

@end

@fs fs_cube

in vec3 col;

out vec4 o_frag_col;

void main() {
	o_frag_col=vec4(col, 1);
}

@end

@program cube vs_cube fs_cube





/*=====DEPTH VIEW SHADER=====*/

@vs vs_depthview

in vec2 i_pos;
in vec2 i_uv;

out vec2 uv;

void main() {
	uv=i_uv;
	gl_Position=vec4(i_pos, .5, 1);
}

@end

@fs fs_depthview

//layout(binding=0) uniform fs_depthview_params {
	//float u_near;
	//float u_far;
//};

@image_sample_type u_depthview_tex depth
layout(binding=0) uniform texture2D u_depthview_tex;
@sampler_type u_depthview_smp comparison
layout(binding=0) uniform sampler u_depthview_smp;

in vec2 uv;

out vec4 o_frag_col;

//float linearizeDepth(float depth) {
	//return u_near*u_far/(u_near+depth*(u_far-u_near));
//}

void main() {
	float d=texture(sampler2D(u_depthview_tex, u_depthview_smp), uv).r;
	o_frag_col=vec4(vec3(d), 1);
}

@end

@program depthview vs_depthview fs_depthview