//encode shadows shader

@vs vs_shadow_map

in vec3 i_pos;
in vec3 i_norm;
in vec2 i_uv;

out vec3 pos;

layout(binding=0) uniform p_vs_shadow_map {
	mat4 u_model;
	mat4 u_mvp;
};

void main() {
	pos=(u_model*vec4(i_pos, 1)).xyz;
	gl_Position=u_mvp*vec4(i_pos, 1);
}

@end

@fs fs_shadow_map

in vec3 pos;

out vec4 o_frag_col;

layout(binding=1) uniform p_fs_shadow_map {
	vec3 u_light_pos;
	float u_cam_near;
	float u_cam_far;
};

//float->int
vec4 encodeFloat(float f) {
	vec4 enc=fract(f*vec4(1., 255., 65025., 16581375.));
	return enc-enc.yzwx*vec4(1./255., 1./255., 1./255., 0.);
}

void main() {
	float dist=length(u_light_pos-pos);
	float dist01=(dist-u_cam_near)/(u_cam_far-u_cam_near);
	o_frag_col=encodeFloat(dist01);
}

@end

@program shadow_map vs_shadow_map fs_shadow_map