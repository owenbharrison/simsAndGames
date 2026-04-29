//basic tinted 3d shader

@vs vs_mesh

in vec3 i_pos;
in vec3 i_norm;
in vec2 i_uv;

layout(binding=0) uniform p_vs_mesh {
	mat4 u_mvp;
};

void main() {
	gl_Position=u_mvp*vec4(i_pos, 1);
}

@end

@fs fs_mesh

out vec4 o_frag_col;

layout(binding=1) uniform p_fs_mesh {
	vec4 u_tint;
};

void main() {
	o_frag_col=u_tint;
}

@end

@program mesh vs_mesh fs_mesh