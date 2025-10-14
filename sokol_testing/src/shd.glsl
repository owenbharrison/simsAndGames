@vs vs
//xyzrgba
layout(location=0) in vec3 pos;
layout(location=1) in vec4 col;

//combine these eventually
layout(binding=0) uniform vs_params{
	mat4 model;
	mat4 view;
	mat4 proj;
};

out vec4 vert_col;

void main() {
	//read this right to left
	gl_Position=proj*view*model*vec4(pos, 1.);
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