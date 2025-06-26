#define FPS 8 //how often angle changes
#define PI 3.1415927
#define SIZE 8 //pixel size of dots
#define BLUR .07 //antialiasing
#define SQRT2 1.4142136

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

//rotate point around origin
vec2 rotVec(vec2 v, float f) {
	float c=cos(f), s=sin(f);
	return v*mat2(c, -s, s, c);
}

//gamma correction
vec3 srgb2lin(vec3 srgb) {
	return pow(srgb, vec3(2.2));
}
vec3 lin2srgb(vec3 rgb) {
	return pow(rgb, vec3(.4545455));
}

//color model conversion
vec4 rgb2cmyk(vec3 rgb){
	float k=1.-max(max(rgb.r, rgb.g), rgb.b);
	return vec4((1-rgb-k)/(1-k), k);
}
vec3 cmyk2rgb(vec4 cmyk){
	return (1-cmyk.xyz)*(1-cmyk.w);
}

void main() {
	//pixel space coordinates
	vec2 resolution=textureSize(tex1, 0);
	vec2 coord=resolution*xUV1;
	
	//get time from top left pixel
	vec4 rgba=texelFetch(tex1, ivec2(0, 0), 0);
	ivec4 byte_vals=ivec4(.5+255*rgba);
	uint frame_count=uint(byte_vals.r)|(uint(byte_vals.g)<<8)|(uint(byte_vals.b)<<16)|(uint(byte_vals.a)<<24);
	float time=frame_count/165.;

	//truncate time
	float rand_seed=round(FPS*time)/FPS;
	
	//random rotations to avoid moire
	float angle_c=PI*hash11(rand_seed);
	float angle_m=angle_c+PI*mix(.2, .25, hash11(10+rand_seed));
	float angle_y=angle_m+PI*mix(.2, .25, hash11(20+rand_seed));
	float angle_k=angle_y+PI*mix(.2, .25, hash11(30+rand_seed));
	
	//radial gradients: translate, rotate, shrink, repeat, normalize
	vec4 dist=vec4(
		length(.5-fract(rotVec(coord-vec2(0, 0)*resolution, angle_c)/SIZE)),
		length(.5-fract(rotVec(coord-vec2(0, 1)*resolution, angle_m)/SIZE)),
		length(.5-fract(rotVec(coord-vec2(1, 0)*resolution, angle_y)/SIZE)),
		length(.5-fract(rotVec(coord-vec2(1, 1)*resolution, angle_k)/SIZE))
	);
	
	//get image color
	vec3 rgb=srgb2lin(texture(tex1, xUV1).rgb);
	
	//convert to cmyk
	vec4 cmyk=rgb2cmyk(rgb);
	
	//thresh gradients based on color
	vec4 thresh=smoothstep(dist-BLUR, dist+BLUR, vec4(
		.5*SQRT2*cmyk.xyz,
		.46*SQRT2*cmyk.w
	));
	
	//convert to rgb
	vec3 final_rgb=cmyk2rgb(thresh);
	pix_out=vec4(lin2srgb(final_rgb), 1);
}