/*=====SKYBOX SHADER=====*/

@vs vs_skybox

layout(binding=0) uniform vs_skybox_params {
	mat4 u_mvp;
};

in vec3 v_pos;
in vec2 v_uv;

out vec2 uv;

void main() {
	//z component=depth=1
	vec4 pos=u_mvp*vec4(v_pos, 1);
	gl_Position=pos.xyww;
	uv=v_uv;
}

@end

@fs fs_skybox

layout(binding=0) uniform texture2D skybox_tex;
layout(binding=0) uniform sampler skybox_smp;

in vec2 uv;

out vec4 frag_color;

void main() {
	frag_color=texture(sampler2D(skybox_tex, skybox_smp), uv);
}

@end

@program skybox vs_skybox fs_skybox





/*=====CUBE SHADER=====*/

@vs vs_cube

layout(binding=0) uniform vs_cube_params {
	mat4 u_model;
	mat4 u_mvp;
};

in vec3 v_pos;
in vec3 v_norm;

out vec3 pos;
out vec3 norm;

void main() {
	gl_Position=u_mvp*vec4(v_pos, 1);

	pos=(u_model*vec4(v_pos, 1)).xyz;

	norm=normalize(mat3(u_model)*v_norm);
}

@end

@fs fs_cube

layout(binding=1) uniform fs_cube_params {
	vec3 u_col;
	vec3 u_light_pos;
	vec3 u_eye_pos;
};

in vec3 pos;
in vec3 norm;

out vec4 frag_col;

void main() {
	vec3 N=normalize(norm);
	vec3 L=normalize(u_light_pos-pos);
	vec3 V=normalize(u_eye_pos-pos);
	vec3 R=reflect(-L, N);
	
	float amb_mag=.3;
	float diff_mag=.7*max(dot(N, L), 0);

	//white specular
	float spec_mag=.3*pow(max(dot(R, V), 0), 32);
	vec3 spec=spec_mag*vec3(1, 1, 1);

	frag_col=vec4(u_col*(amb_mag+diff_mag)+spec, 1);
}

@end

@program cube vs_cube fs_cube
