const float Curvature=4.4;
const float Blur=.016;
const float ChromaticAbberation=1.007;

void main() {
	vec2 uv=xUV1;

	//curving
	vec2 crt_uv=2*uv-1;
	vec2 offset=crt_uv.yx/Curvature;
	crt_uv+=crt_uv*offset*offset;
	crt_uv=.5+.5*crt_uv;

	//border
	vec2 edge=smoothstep(0, Blur, crt_uv)*(1-smoothstep(1-Blur, 1, crt_uv));
	
	//chromatic abberation
	pix_out=vec4(edge.x*edge.y*vec3(
		texture(tex1, .5+(crt_uv-.5)*ChromaticAbberation).r,
		texture(tex1, crt_uv).g,
		texture(tex1, .5+(crt_uv-.5)/ChromaticAbberation).b
	), 1);
	
	//scanlines
	vec2 coord=xUV1*textureSize(tex1, 0);
	if(mod(coord.y, 2)<1) pix_out.rgb*=.7;
	else if(mod(coord.x, 3)<1) pix_out.rgb*=.7;
	else pix_out.rgb*=1.2;
}