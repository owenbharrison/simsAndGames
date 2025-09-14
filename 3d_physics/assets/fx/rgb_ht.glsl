#define SIZE 6

#define FPS 8

#define BLUR .14

#define LOW .12
#define HIGH .85

#define SPREAD .087

//https://www.shadertoy.com/view/4djSRW
float hash11(float p) {
	p=fract(.1031*p);
	p*=33.33+p;
	return fract(2*p*p);
}
vec3 hash32(vec2 p) {
	vec3 p3=fract(vec3(.1031, .1030, .0973)*p.xyx);
	p3+=dot(p3, 33.33+p3.yxz);
	return fract(p3.zyx*(p3.xxy+p3.yzz));
}

const float Pi=3.1415927;
vec2 rotVec(vec2 v, float f) {
	float c=cos(f), s=sin(f);
	return v*mat2(c, -s, s, c);
}

void main() {
	//pixel space coordinates
	vec2 resolution=textureSize(tex1, 0);
	vec2 coord=resolution*xUV1;
	
	//get time from top left pixel
	vec4 rgba=texelFetch(tex1, ivec2(0, 0), 0);
	ivec4 byte_vals=ivec4(.5+255*rgba);
	uint ms_count=uint(byte_vals.r)|(uint(byte_vals.g)<<8)|(uint(byte_vals.b)<<16)|(uint(byte_vals.a)<<24);
	float time=ms_count/1000.;
	
	//randomly offsetted to avoid moire
	float rand_seed=round(FPS*time)/FPS;
	float angle_r=Pi*hash11(rand_seed);
	float angle_g=angle_r+Pi*mix(.25, .33, hash11(10+rand_seed));
	float angle_b=angle_g+Pi*mix(.25, .33, hash11(20+rand_seed));
	
	//radial gradients: rotate, shrink, repeat, normal
	vec2 ctr_coord=.5*resolution-coord;
	vec3 dist=vec3(
		length(.5-fract(rotVec(ctr_coord, angle_r)/SIZE)),
		length(.5-fract(rotVec(ctr_coord, angle_g)/SIZE)),
		length(.5-fract(rotVec(ctr_coord, angle_b)/SIZE))
	);
	
	//halftone thresholding
	vec3 col=.7*texture(tex1, xUV1).rgb;
	pix_out=vec4(smoothstep(BLUR, -BLUR, dist-col), 1);
	
	//limit color range
	pix_out.rgb=mix(vec3(LOW), vec3(HIGH), pix_out.rgb);

	//add some noise
	pix_out.rgb+=SPREAD*(.5-hash32(coord));

	//apply vignette https://www.shadertoy.com/view/lsKSWR
  //vec2 vig=xUV1*(1-xUV1.yx);
  //pix_out.rgb*=pow(30*vig.x*vig.y, .32);
}