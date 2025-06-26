//sobel simplification: transpose x for y!
const int sobel[9]=int[9](-1,0,1,-2,0,2,-1,0,1);

//three phase shifted sine waves
const float Pi=3.1415927;
vec3 colorWheel(float angle) {
	return .5+.5*cos(angle+Pi*vec3(0, .667, -.667));
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

	//accumulate convolution
	vec2 sum=vec2(0);
	for(int i=0; i<3; i++) {
		for(int j=0; j<3; j++) {
			vec2 offset_uv=vec2(i, j)/resolution;
			vec4 col=texture(tex1, xUV1+offset_uv);

			//luminance formula
			float lum=dot(col.rgb, vec3(.299, .587, .114));
			sum+=lum*vec2(sobel[i+3*j], sobel[j+3*i]);
		}
	}
	float mag=length(sum);

	//find edge angle
	float angle=time+atan(sum.y, sum.x);
	pix_out=vec4(mag*colorWheel(angle), 1);
}