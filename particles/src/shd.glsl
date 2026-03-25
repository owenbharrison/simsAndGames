@vs vs_quad

in vec2 i_uv;
in vec2 inst_pos;
in vec2 inst_size;
in vec3 inst_col;

out vec2 uv;
out vec4 col;

layout(binding=0) uniform p_vs_quad {
	vec2 u_resolution;
	vec2 u_cam;
	float u_zoom;
};

void main() {
	uv=i_uv;
	col=vec4(inst_col, 1);
	//make quad with uv
	vec2 wld=inst_pos+i_uv*inst_size;
	//world to screen
	vec2 scr=.5*u_resolution+u_zoom*(wld-u_cam);
	//normalize
	vec2 ndc=2*scr/u_resolution-1;
	ndc.y*=-1;
	gl_Position=vec4(ndc, .5, 1);
}

@end

@fs fs_quad

in vec2 uv;
in vec4 col;

out vec4 o_frag_col;

layout(binding=1) uniform texture2D b_quad_tex;
layout(binding=1) uniform sampler b_quad_smp;

void main() {
	vec4 tex_col=texture(sampler2D(b_quad_tex, b_quad_smp), uv);
	o_frag_col=col*tex_col;
}

@end

@program quad vs_quad fs_quad





@vs vs_line

in float i_t;

out vec4 col;

layout(binding=0) uniform p_vs_line {
	vec3 u_col;
	vec2 u_a;
	vec2 u_b;
};

void main() {
	col=vec4(u_col, 1);
	vec2 pos=u_a+i_t*(u_b-u_a);
	vec2 ndc=2*pos-1;
	ndc.y*=-1;
	gl_Position=vec4(ndc, .5, 1);
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