//3d line shader

@vs vs_line

in vec3 i_pos;
in vec4 i_col;

out vec4 col;

layout(binding=0) uniform p_vs_line {
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

layout(binding=1) uniform p_fs_line {
	vec4 u_tint;
};

void main() {
	o_frag_col=u_tint*col;
}

@end

@program line vs_line fs_line