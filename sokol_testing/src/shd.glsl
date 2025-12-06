@vs vs
layout(binding=0) uniform vs_params{
	mat4 u_model;
	mat4 u_proj_view;
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
	gl_Position=u_proj_view*world_pos;
	v_uv=uv;
}
@end

@fs fs
layout(binding=1) uniform fs_params{
	vec3 u_view_pos;
	int u_num_lights;
	vec4 u_light_pos[16];
	vec4 u_light_col[16];
};

in vec3 v_pos;
in vec3 v_norm;
in vec2 v_uv;

layout(binding=0) uniform texture2D u_tex;
layout(binding=0) uniform sampler u_smp;

out vec4 frag_col;

void main() {
	const float amb_mag=1;//0-1
	const vec3 amb_col=vec3(0.03);
	
	const float shininess=64;//32-256
	const float spec_mag=1;//0-1

	const float att_k0=1.0;
	const float att_k1=0.09;
	const float att_k2=0.032;

	//srgb->linear
	vec3 base_col_srgb=texture(sampler2D(u_tex, u_smp), v_uv).rgb;
	vec3 base_col=pow(base_col_srgb, vec3(2.2));

	vec3 n=normalize(v_norm);
	vec3 v=normalize(u_view_pos-v_pos);

	//start with ambient
	vec3 col=amb_col*base_col*amb_mag;
	for(int i=0; i<u_num_lights; i++) {
		vec3 l_pos=u_light_pos[i].xyz;
		vec3 l_col=u_light_col[i].rgb;

		vec3 l=l_pos-v_pos;
		float dist=length(l);
		l/=dist;

		//diffuse(Lambert)
		float n_dot_l=max(0, dot(n, l));
		vec3 diffuse=l_col*base_col*n_dot_l;

		//blinn-phong specular
		vec3 h=normalize(l+v);//half-vector
		float n_dot_h=max(0, dot(n, h));
		float spec_factor=pow(n_dot_h, shininess);
		vec3 specular=l_col*spec_factor*spec_mag;

		//attenuation (apply equally to all channels)
		float att=1/(att_k0+att_k1*dist+att_k2*dist*dist);

		col+=att*(diffuse+specular);
	}
	
	//linear->srgb
	vec3 col_srgb=pow(col, vec3(1/2.2));
	frag_col=vec4(col_srgb, 1);
}
@end

@program shd vs fs