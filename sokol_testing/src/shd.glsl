@vs vs
layout(binding=0) uniform vs_params{
	mat4 u_model;
	mat4 u_view_proj;
};

in vec3 pos;
in vec3 norm;
in vec2 uv;

out vec3 v_pos;
out vec3 v_norm;
out vec2 v_uv;

void main() {
	vec4 world_pos=u_model*vec4(pos, 1);
	v_pos=world_pos.xyz;
	v_norm=mat3(u_model)*norm;
	gl_Position=u_view_proj*world_pos;
	v_uv=uv;
}
@end

@fs fs
layout(binding=1) uniform fs_params{
	vec3 u_light_pos;
	vec3 u_cam_pos;
};

in vec3 v_pos;
in vec3 v_norm;
in vec2 v_uv;

layout(binding=0) uniform texture2D u_tex;
layout(binding=0) uniform sampler u_smp;

out vec4 frag_col;

void main() {
	vec3 N=normalize(v_norm);
	vec3 L=normalize(u_light_pos-v_pos);
	vec3 V=normalize(u_cam_pos-v_pos);
	vec3 R=reflect(-L, N);

	//diffuse based on tex col?
	float amb_mag=.1;
	float diff_mag=.8*max(dot(N, L), 0);
	vec4 col=texture(sampler2D(u_tex, u_smp), v_uv);
	vec3 amb=amb_mag*col.rgb;
	vec3 diff=diff_mag*col.rgb;

	//white specular
	float spec_mag=.3*pow(max(dot(R, V), 0), 32);
	vec3 spec=spec_mag*vec3(1, 1, 1);

	frag_col=vec4(amb+diff+spec, 1);
}
@end

@program shd vs fs