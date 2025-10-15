@vs vs
//xyzrgba
layout(location=0) in vec3 pos;
layout(location=1) in vec4 col;

layout(binding=0) uniform vs_params{
	mat4 mvp;
};

out vec4 vert_col;

void main() {
	//read this right to left
	gl_Position=mvp*vec4(pos, 1.);
	vert_col=col;
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