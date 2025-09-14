const float Pi=3.1415927;

const int FramesPerSecond=4;

const float VoronoiScale=13.6;

#define LUM(x) dot(x, vec3(.299, .587, .114))

const int NumLevels=4;

const float LineWidth=4.7;

//https://www.shadertoy.com/view/4djSRW
float hash11(float p) {
	p=fract(.1031*p);
	p*=33.33+p;
	return fract(2*p*p);
}
vec2 hash21(float p) {
	vec3 p3=fract(p*vec3(.1031, .1030, .0973));
	p3+=dot(p3, 33.33+p3.yzx);
	return fract(p3.zy*(p3.xx+p3.yz));
}
vec2 hash22(vec2 p) {
	vec3 p3=fract(p.xyx*vec3(.1031, .1030, .0973));
	p3+=dot(p3, 33.33+p3.yzx);
	return fract(p3.zy*(p3.xx+p3.yz));
}

//https://www.shadertoy.com/view/MslGD8
vec2 voronoi(vec2 x) {
	vec2 n=floor(x);
	vec2 f=x-n;

	vec3 m=vec3(8);
	for(int j=-1; j<=1; j++) {
		for(int i=-1; i<=1; i++) {
			vec2 g=vec2(i, j);
			vec2 o=hash22(n+g);
			vec2 r=g-f+(.5+.5*sin(2*Pi*o));
			float d=dot(r, r);
			if(d<m.x) m=vec3(d, o);
		}
	}

	return vec2(sqrt(m.x), m.y+m.z);
}

//sobel simplification: transpose x for y!
const int sobel[9]=int[9](-1,0,1,-2,0,2,-1,0,1);

void main() {
	//pixel space coordinates
	vec2 resolution=textureSize(tex1, 0);
	vec2 coord=resolution*xUV1;
	
	//get time from top left pixel
	vec4 rgba=texelFetch(tex1, ivec2(0, 0), 0);
	ivec4 byte_vals=ivec4(.5+255*rgba);
	uint ms_count=uint(byte_vals.r)|(uint(byte_vals.g)<<8)|(uint(byte_vals.b)<<16)|(uint(byte_vals.a)<<24);
	float time=ms_count/1000.;
	
	//get random voronoi region
	vec2 p=coord/max(resolution.x, resolution.y);
	//truncate time
	time=FramesPerSecond*round(FramesPerSecond*time);
	vec2 c=voronoi(VoronoiScale*(p+hash21(time)));

	//sample image and get luminance
	vec4 base=texture(tex1, xUV1);
	float lum=LUM(base.rgb);

	//posterize to determine # hatch
	int num=NumLevels-int(float(1+NumLevels)*lum);
	//offset coord a bit for jagged lines
	vec2 rand_coord=coord-.5+.4*hash22(coord);
	float rand_seed=100*c.y;
	float angle=2*Pi*hash11(rand_seed);
	float new_lum=1.;
	for(int i=0; i<num; i++) {
		//offset angle to avoid moire
		rand_seed+=10;
		angle+=Pi*mix(
			1/float(1+num),
			1/float(num),
			hash11(rand_seed)
		);
		
		//procedural hatch pattern
		float d2l=abs(dot(rand_coord, vec2(cos(angle), sin(angle))));
		float repeat=abs(1-2*mod(d2l/LineWidth, 1));
		repeat=pow(repeat, 1.3);
		float line=smoothstep(.64, .65, repeat);
		new_lum-=line;
	}
	pix_out=vec4(base.rgb*clamp(new_lum, 0, 1), 1);
	
	//add edge lines w/ sobel
	vec2 rand_uv=rand_coord/resolution;
	vec2 sum=vec2(0);
	for(int i=0; i<3; i++) {
		for(int j=0; j<3; j++) {
			vec2 offset_uv=vec2(i, j)/resolution;
			vec4 col=texture(tex1, rand_uv+offset_uv);

			float lum=LUM(col.rgb);
			sum+=lum*vec2(sobel[i+3*j], sobel[j+3*i]);
		}
	}
	float mag=length(sum);
	pix_out.rgb*=1-smoothstep(.75, .9, mag);
}