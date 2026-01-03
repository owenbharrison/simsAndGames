/*===== RENDER SHADER =====*/

@vs vs_render

layout(binding=0) uniform vs_render_params {
	mat4 u_mvp;
};

in vec3 i_pos;
in vec3 i_col;

out vec3 col;

void main() {
	gl_Position=u_mvp*vec4(i_pos, 1);
	col=i_col;
}

@end

@fs fs_render

in vec3 col;

out vec4 o_frag_col;

void main() {
	o_frag_col=vec4(col, 1);
}

@end

@program render vs_render fs_render





/*===== DISPLAY SHADER =====*/

@vs vs_display

in vec2 i_pos;
in vec2 i_uv;

out vec2 uv;

void main() {
	gl_Position=vec4(i_pos, .5, 1);
	uv=i_uv;
}

@end

@fs fs_display

layout(binding=0) uniform sampler u_display_smp;
layout(binding=0) uniform texture2D u_display_tex;

in vec2 uv;

out vec4 o_frag_col;

const float Curvature=5.1;
const float Blur=.012;
const float CAAmt=1.012;

void main() {
	//curving
	vec2 norm_uv=2*uv-1;
	vec2 offset=norm_uv.yx/Curvature;
	vec2 crt_uv=.5+.5*(1+offset*offset)*norm_uv;

	//border
	vec2 edge=smoothstep(0., Blur, crt_uv)*(1.-smoothstep(1.-Blur, 1., crt_uv));

	//chromatic abberation
	o_frag_col=vec4(edge.x*edge.y*vec3(
		texture(sampler2D(u_display_tex, u_display_smp), (crt_uv-.5)*CAAmt+.5).r,
		texture(sampler2D(u_display_tex, u_display_smp), crt_uv).g,
		texture(sampler2D(u_display_tex, u_display_smp), (crt_uv-.5)/CAAmt+.5).b
	), 1);

	//scanlines
	float scan=.75*.5*abs(sin(gl_FragCoord.y));
	o_frag_col.rgb=mix(o_frag_col.rgb, vec3(0), scan);
}

@end

@program display vs_display fs_display