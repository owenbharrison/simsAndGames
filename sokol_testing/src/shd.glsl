@block uniforms
layout(binding=0) uniform vs_params {
    mat4 u_mvp;
};
@end

//offscreen rendering shaders
@vs vs_offscreen
@include_block uniforms

in vec3 v_pos;
in vec3 v_norm;
in vec2 v_uv;

out vec3 norm;

void main() {
	gl_Position=u_mvp*vec4(v_pos, 1);
	norm=v_norm;
}
@end

@fs fs_offscreen
in vec3 norm;
out vec4 frag_color;

void main() {
	frag_color=vec4(.5+.5*norm, 1);
}
@end

@program offscreen vs_offscreen fs_offscreen

//default-pass shaders
@vs vs_default
@include_block uniforms


in vec3 v_pos;
in vec3 v_norm;
in vec2 v_uv;

out vec3 norm;
out vec2 uv;

void main() {
	gl_Position=u_mvp*vec4(v_pos, 1);
	uv=v_uv;
	norm=(u_mvp*vec4(v_norm, 1)).xyz;
}
@end

@fs fs_default
layout(binding=0) uniform texture2D u_tex;
layout(binding=0) uniform sampler u_smp;

in vec3 norm;
in vec2 uv;

out vec4 frag_color;

void main() {
	vec4 c=texture(sampler2D(u_tex, u_smp), vec2(20, 10)*uv);
	vec3 light_dir=normalize(vec3(1, 1, -1));
	float l=2*clamp(dot(norm, light_dir), 0, 1);
	frag_color=vec4((.25+l)*c.rgb, 1);
}
@end

@program default vs_default fs_default