/*==== COLORVIEW SHADER ====*/

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