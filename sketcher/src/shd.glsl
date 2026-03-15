/*==== LINE SHADER ====*/

@vs vs_line

in vec2 i_pos;

layout(binding=0) uniform p_vs_line {
	vec3 u_mvp_c0, u_mvp_c1, u_mvp_c2;
};

void main() {
	mat3 m=mat3(u_mvp_c0, u_mvp_c1, u_mvp_c2);
	gl_Position=vec4((m*vec3(i_pos, 1)).xy, .5, 1);
}

@end

@fs fs_line

out vec4 o_frag_col;

layout(binding=1) uniform p_fs_line {
	vec4 u_col;
};

void main() {
	o_frag_col=u_col;
}

@end

@program line vs_line fs_line





/*==== TRI SHADER ====*/

@vs vs_tri

in vec2 i_pos;

layout(binding=0) uniform p_vs_tri {
	vec3 u_mvp_c0, u_mvp_c1, u_mvp_c2;
};

void main() {
	mat3 m=mat3(u_mvp_c0, u_mvp_c1, u_mvp_c2);
	gl_Position=vec4((m*vec3(i_pos, 1)).xy, .5, 1);
}

@end

@fs fs_tri

out vec4 o_frag_col;

layout(binding=1) uniform p_fs_tri {
	vec4 u_col;
};

void main() {
	o_frag_col=u_col;
}

@end

@program tri vs_tri fs_tri





/*=====OUTLINE SHADER=====*/

@vs vs_outline

in vec2 i_pos;

void main() {
	gl_Position=vec4(i_pos, .5, 1);
}

@end

@fs fs_outline

out vec4 o_frag_col;

layout(binding=0) uniform texture2D b_outline_tex;
layout(binding=0) uniform sampler b_outline_smp;

layout(binding=0) uniform p_fs_outline {
	vec2 u_resolution;
	float u_rad;
	vec3 u_col;
};

void main() {
	vec2 uv=gl_FragCoord.xy/u_resolution;

	vec4 base_col=texture(
		sampler2D(b_outline_tex, b_outline_smp),
		uv
	);

	//alpha determines "layer"
	if(base_col.a<.5) {
		//check neighbors
		int r=int(ceil(u_rad));
		for(int i=-r; i<=r; i++) {
			for(int j=-r; j<=r; j++) {
				//circular kernel
				vec2 offset=vec2(i, j);
				if(dot(offset, offset)>u_rad*u_rad) continue;

				vec4 col=texture(
					sampler2D(b_outline_tex, b_outline_smp),
					uv+offset/u_resolution
				);

				//close to edge
				if(col.a>.5) {
					o_frag_col=vec4(u_col, 1);
					return;
				}
			}
		}
	}
	
	//default
	o_frag_col=vec4(base_col.rgb, 1);
}

@end

@program outline vs_outline fs_outline