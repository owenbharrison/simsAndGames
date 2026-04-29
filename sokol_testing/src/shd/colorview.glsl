//view the color of a texture with some tinting

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