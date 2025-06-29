//https://www.shadertoy.com/view/4djSRW
vec2 hash22(vec2 p) {
	vec3 p3=fract(p.xyx*vec3(.1031, .1030, .0973));
	p3+=dot(p3, 33.33+p3.yzx);
	return fract(p3.zy*(p3.xx+p3.yz));
}

//https://www.shadertoy.com/view/MslGD8
vec2 voronoi(float time, vec2 x) {
	vec2 n=floor(x), f=x-n;

	float record=8.;
	vec2 close_pt;
	for (int j=-1; j<=1; j++) {
		for (int i=-1; i<=1; i++) {
			vec2 g=vec2(i, j);
			vec2 o=hash22(n+g);
			vec2 offset=.5+.5*sin(time+6.2831*o);
			vec2 r=g-f+offset;
			float d=dot(r, r);
			if(d<record) record=d, close_pt=n+g+offset;
		}
	}

	return close_pt;
}

const float Scale=100;

void main() {
	//pixel space coordinates
	vec2 resolution=textureSize(tex1, 0);
	vec2 coord=resolution*xUV1;
	
	//get time from top left pixel
	vec4 rgba=texelFetch(tex1, ivec2(0, 0), 0);
	ivec4 byte_vals=ivec4(.5+255*rgba);
	uint frame_count=uint(byte_vals.r)|(uint(byte_vals.g)<<8)|(uint(byte_vals.b)<<16)|(uint(byte_vals.a)<<24);
	float time=frame_count/165.;

	//transform into and out of voronoi space
	float scl=Scale/max(resolution.x, resolution.y);
	vec2 close_pt=voronoi(time, scl*coord)/scl;
	vec2 uv=close_pt/resolution;
	
	//nearest neighbor sampling?
	ivec2 tex_size=textureSize(tex1, 0);
	ivec2 tex_coord=ivec2(uv*vec2(tex_size));
	tex_coord=clamp(tex_coord, ivec2(0, 0), tex_size-1);
	pix_out=texelFetch(tex1, tex_coord, 0);
}