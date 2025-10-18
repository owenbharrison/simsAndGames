@vs vs
layout(binding=0) uniform vs_params{
	mat4 mvp;
};

in vec3 vert_pos;
in vec4 vert_col0;
in vec3 cube_pos;
in vec3 cube_rot;

out vec4 vert_col;

mat3 makeRotX(float r) {
	float c=cos(r), s=sin(r);
	return mat3(
		vec3(1,  0, 0),
		vec3(0,  c, s),
		vec3(0, -s, c)
	);
}

mat3 makeRotY(float r) {
	float c=cos(r), s=sin(r);
	return mat3(
		vec3(c, 0, -s),
		vec3(0, 1,  0),
		vec3(s, 0,  c)
	);
}

mat3 makeRotZ(float r) {
	float c=cos(r), s=sin(r);
	return mat3(
		vec3( c, s, 0),
		vec3(-s, c, 0),
		vec3( 0, 0, 1)
	);
}

void main() {
	mat3 rot_x=makeRotX(cube_rot.x);
	mat3 rot_y=makeRotY(cube_rot.y);
	mat3 rot_z=makeRotZ(cube_rot.z);
	mat3 rot=rot_z*rot_y*rot_x;
	gl_Position=mvp*vec4(cube_pos+rot*vert_pos, 1);
	vert_col=vert_col0;
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