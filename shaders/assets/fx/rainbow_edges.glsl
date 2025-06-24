const int sobel[9]=int[9](-1,0,1,-2,0,2,-1,0,1);

//three phase shifted sine waves
const float Pi=3.1415927;
vec3 colorWheel(float angle) {
	return .5+.5*cos(angle+Pi*vec3(0,.667,-.667));
}

void main() {
	vec2 sz=1./vec2(textureSize(tex1, 0));

	vec2 sum=vec2(0);
	//accumulate convolution
	for(int i=0; i<3; i++) {
		for(int j=0; j<3; j++) {
			vec2 offset_uv=sz*vec2(i, j);
			vec4 col=texture(tex1, xUV1+offset_uv);

			//luminance formula
			float lum=dot(col.rgb, vec3(.299, .587, .114));
			sum+=lum*vec2(sobel[i+3*j], sobel[j+3*i]);
		}
	}
	float mag=length(sum);
	//which angle is edge?
	float angle=atan(sum.y, sum.x);
	pix_out=vec4(mag*colorWheel(angle), 1);
}