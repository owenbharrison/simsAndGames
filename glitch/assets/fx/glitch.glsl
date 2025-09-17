//shadertoy.com/view/4djSRW
float hash11(float p) {
	p=fract(.1031*p);
	p*=33.33+p;
	return fract(2.*p*p);
}

//https://www.youtube.com/watch?v=lctXaT9pxA0
float bias(float x, float bias) {
	float k=pow(1.-bias, 3.);
	//shadertoy.com/view/Xd2yRd
	return x*k/(1.+x*(k-1.));
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

	//which region are we in
	int y=int(coord.y), ry=y;
	//change seed every nth of a second
	int i=100*int(6.*time);
	while(true) {
		//get region size
		float t=hash11(float(i));
		//make bigger less common
		t=bias(t, .7);
		//10-90 pixels
		int h=10+int(90.*t);
		
		if(ry>h) {
			//goto next region
			ry-=h;
			i++;
		} else break;
	}
	
	//get region offset
	//-4 to 4 pixels
	int off=4-int(8.*hash11(100.+float(i)));
	int x=off+int(coord.x);
	
	//get color
	//with chromatic aberration
	pix_out=vec4(
		texture(tex1, vec2(x-1, y)/resolution).r,
		texture(tex1, vec2(x, y)/resolution).g,
		texture(tex1, vec2(x+1, y)/resolution).b,
		1
	);
	
	//vignette
	//shadertoy.com/view/lsKSWR
	vec2 vig=xUV1*(1.-xUV1.yx);
	pix_out.rgb*=pow(45.*vig.x*vig.y, .25);
}