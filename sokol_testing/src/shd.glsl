//common uniforms across all vertex shaders
@block vs_uniforms
layout(binding=0) uniform vs_params {
	mat4 u_mvp;
};
@end





/*====="SHADOW" SHADER=====*/

@vs vs_shadow

@include_block vs_uniforms

in vec3 v_pos;
in vec3 v_norm;
in vec2 v_uv;

void main() {
	gl_Position=u_mvp*vec4(v_pos, 1);
}

@end

@fs fs_shadow

out vec4 frag_color;

void main() {
	frag_color=vec4(1, 1, 1, 1);
}

@end

@program shadow vs_shadow fs_shadow





/*=====SKYBOX SHADER=====*/

@vs vs_skybox

@include_block vs_uniforms

in vec3 v_pos;
in vec3 v_norm;
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





/*=====DEFAULT TEXTURING RENDERING SHADER=====*/

@vs vs_default

@include_block vs_uniforms

in vec3 v_pos;
in vec3 v_norm;
in vec2 v_uv;

out vec2 uv;

void main() {
	gl_Position=u_mvp*vec4(v_pos, 1);
	uv=v_uv;
}

@end

@fs fs_default

layout(binding=0) uniform texture2D default_tex;
layout(binding=0) uniform sampler default_smp;

in vec2 uv;

out vec4 frag_color;

void main() {
	frag_color=texture(sampler2D(default_tex, default_smp), uv);
}

@end

@program default vs_default fs_default





/*=====LINE RENDERING SHADER=====*/

@vs vs_line

@include_block vs_uniforms

in vec3 v_pos;
in vec4 v_col;

out vec4 col;

void main() {
	gl_Position=u_mvp*vec4(v_pos, 1);
	col=v_col;
}

@end

@fs fs_line

in vec4 col;

out vec4 frag_color;

void main() {
	frag_color=col;
}

@end

@program line vs_line fs_line





/*=====DEBUG TEXTURE VIEW SHADER=====*/

@vs vs_texview

in vec2 v_pos;
in vec2 v_uv;

out vec2 uv;

void main() {
	gl_Position=vec4(v_pos, .5, 1);
	uv=v_uv;
}

@end

@fs fs_texview

layout(binding=0) uniform texture2D texview_tex;
layout(binding=0) uniform sampler texview_smp;

in vec2 uv;

out vec4 frag_color;

void main() {
	vec4 col=texture(sampler2D(texview_tex, texview_smp), uv);
	frag_color=vec4(col.rgb, 1);
}

@end

@program texview vs_texview fs_texview