/*==== LINE SHADER ====*/

@vs vs_line

in vec3 i_pos;

layout(binding=0) uniform vs_line_params {
	mat4 u_mvp;
};

void main() {
	gl_Position=u_mvp*vec4(i_pos, 1);
}

@end

@fs fs_line

out vec4 o_frag_col;

layout(binding=1) uniform fs_line_params {
	vec4 u_col;
};

void main() {
	o_frag_col=u_col;
}

@end

@program line vs_line fs_line





/*==== MESH SHADER ====*/

@vs vs_mesh

in vec3 i_pos;
in vec3 i_norm;
in vec2 i_uv;

out vec3 pos;
out vec3 norm;
out vec2 uv;

layout(binding=0) uniform vs_mesh_params {
	mat4 u_model;
	mat4 u_mvp;
	vec2 u_tl, u_br;
};

void main() {
	pos=(u_model*vec4(i_pos, 1)).xyz;
	norm=normalize(mat3(u_model)*i_norm);
	uv=u_tl+i_uv*(u_br-u_tl);
	gl_Position=u_mvp*vec4(i_pos, 1);
}

@end

@fs fs_mesh

in vec3 pos;
in vec3 norm;
in vec2 uv;

out vec4 o_frag_col;

layout(binding=1) uniform fs_mesh_params {
	vec3 u_eye_pos;
	vec3 u_light_pos;
	vec3 u_tint;
};

layout(binding=0) uniform texture2D u_mesh_tex;
layout(binding=0) uniform sampler u_mesh_smp;

void main() {
	vec3 N=normalize(norm);
	vec3 L=normalize(u_light_pos-pos);
	vec3 V=normalize(u_eye_pos-pos);
	vec3 R=reflect(-L, N);

	float amb_mag=.2;
	float diff_mag=.7*max(dot(N, L), 0);
	vec4 tex_col=texture(sampler2D(u_mesh_tex, u_mesh_smp), uv);
	vec3 base_col=u_tint*tex_col.rgb;

	//white specular
	float spec_mag=.3*pow(max(dot(R, V), 0), 32);
	vec3 spec=spec_mag*vec3(1, 1, 1);

	o_frag_col=vec4(base_col*(amb_mag+diff_mag)+spec, 1);
}

@end

@program mesh vs_mesh fs_mesh





/*==== BILLBOARD SHADER ====*/

@vs vs_billboard

in vec3 i_pos;
in vec2 i_uv;

out vec2 uv;

layout(binding=0) uniform vs_billboard_params {
	mat4 u_mvp;
	vec2 u_tl, u_br;
};

void main() {
	uv=u_tl+i_uv*(u_br-u_tl);
	gl_Position=u_mvp*vec4(i_pos, 1);
}

@end

@fs fs_billboard

in vec2 uv;

out vec4 o_frag_col;

layout(binding=1) uniform fs_billboard_params {
	vec4 u_tint;
};

layout(binding=0) uniform texture2D u_billboard_tex;
layout(binding=0) uniform sampler u_billboard_smp;

void main() {
	vec4 tex_col=texture(sampler2D(u_billboard_tex, u_billboard_smp), uv);
	o_frag_col=u_tint*tex_col;
}

@end

@program billboard vs_billboard fs_billboard





/*==== COLORVIEW SHADER ====*/

@vs vs_colorview

in vec2 i_pos;
in vec2 i_uv;

out vec2 uv;

layout(binding=0) uniform vs_colorview_params {
	vec2 u_tl;
	vec2 u_br;
};

void main() {
	uv=u_tl+i_uv*(u_br-u_tl);
	gl_Position=vec4(i_pos, .5, 1);
}

@end

@fs fs_colorview

in vec2 uv;

out vec4 o_frag_col;

layout(binding=1) uniform fs_colorview_params {
	vec4 u_tint;
};

layout(binding=0) uniform texture2D u_colorview_tex;
layout(binding=0) uniform sampler u_colorview_smp;

void main() {
	vec4 col=texture(sampler2D(u_colorview_tex, u_colorview_smp), uv);
	o_frag_col=u_tint*col;
}

@end

@program colorview vs_colorview fs_colorview





/*==== CRT SHADER ====*/

@vs vs_crt

in vec2 i_pos;

void main() {
	gl_Position=vec4(i_pos, .5, 1);
}

@end

@fs fs_crt

out vec4 o_frag_col;

layout(binding=0) uniform sampler u_crt_smp;
layout(binding=0) uniform texture2D u_crt_tex;

layout(binding=0) uniform fs_crt_params {
	vec2 u_resolution;
};

const float Curvature=5.1;
const float Blur=.012;
const float CAAmt=1.012;

void main() {
	vec2 uv=gl_FragCoord.xy/u_resolution;

	//curving
	vec2 norm_uv=2*uv-1;
	vec2 offset=norm_uv.yx/Curvature;
	vec2 crt_uv=.5+.5*(1+offset*offset)*norm_uv;

	//border
	vec2 edge=smoothstep(0., Blur, crt_uv)*(1.-smoothstep(1.-Blur, 1., crt_uv));

	//chromatic abberation
	o_frag_col=vec4(edge.x*edge.y*vec3(
		texture(sampler2D(u_crt_tex, u_crt_smp), (crt_uv-.5)*CAAmt+.5).r,
		texture(sampler2D(u_crt_tex, u_crt_smp), crt_uv).g,
		texture(sampler2D(u_crt_tex, u_crt_smp), (crt_uv-.5)/CAAmt+.5).b
	), 1);

	//scanlines
	float scan=.75*.5*abs(sin(gl_FragCoord.y));
	o_frag_col.rgb=mix(o_frag_col.rgb, vec3(0), scan);
}

@end

@program crt vs_crt fs_crt