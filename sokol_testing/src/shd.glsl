@vs vs
layout(binding=0) uniform vs_params{
	mat4 mvp;
};

in vec3 pos;
in vec4 col0;
in vec3 inst_pos;

out vec4 vert_col;

void main() {
	//read this right to left
	gl_Position=mvp*vec4(pos+inst_pos, 1.);
	vert_col=col0;
}
@end

@fs fs
in vec4 vert_col;

out vec4 frag_col;

void main() {
	frag_col=vert_col;
}
@end

@program shd vs fs