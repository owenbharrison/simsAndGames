//simple general vert shader
@vs vs_post_process
in vec2 i_pos;
in vec2 i_uv;
out float time;
out vec2 resolution;

layout(binding=0) uniform p_vs_post_process {
	float u_time;
	vec2 u_resolution;
};

void main() {
	time=u_time;
	resolution=u_resolution;
	gl_Position=vec4(i_pos, .5, 1.);
}
@end

@block utils
const float Pi=3.1415927;

//https://www.shadertoy.com/view/4djSRW
float hash11(float p) {
	p=fract(.1031*p);
	p*=33.33+p;
	return fract(2.*p*p);
}
float hash13(vec3 p3) {
	p3=fract(.1031*p3);
	p3+=dot(p3, 33.33+p3.zyx);
	return fract(p3.z*(p3.x+p3.y));
}
vec2 hash21(float p) {
	vec3 p3=fract(vec3(.1031, .1030, .0973)*p);
	p3+=dot(p3, 33.33+p3.yzx);
	return fract(p3.zy*(p3.xx+p3.yz));
}
vec2 hash22(vec2 p) {
	vec3 p3=fract(vec3(.1031, .1030, .0973)*p.xyx);
	p3+=dot(p3, 33.33+p3.yzx);
	return fract(p3.zy*(p3.xx+p3.yz));
}
vec3 hash32(vec2 p) {
	vec3 p3=fract(vec3(.1031, .1030, .0973)*p.xyx);
	p3+=dot(p3, 33.33+p3.yxz);
	return fract(p3.zyx*(p3.xxy+p3.yzz));
}

#define LUM(x) dot(x, vec3(.299, .587, .114))
@end

//identity
@fs fs_identity

in float time;
in vec2 resolution;
out vec4 o_frag_col;

layout(binding=0) uniform texture2D b_identity_tex;
layout(binding=0) uniform sampler b_identity_smp;

void main() {
	vec2 uv=gl_FragCoord.xy/resolution;

	o_frag_col=texture(sampler2D(b_identity_tex, b_identity_smp), uv);
}

@end

@program identity vs_post_process fs_identity

//crt monitor
@fs fs_crt

in float time;
in vec2 resolution;
out vec4 o_frag_col;

layout(binding=0) uniform texture2D b_crt_tex;
layout(binding=0) uniform sampler b_crt_smp;

const float Curvature=5.1;
const float Blur=.014;
const float CAScale=1.009;

void main() {
	vec2 uv=gl_FragCoord.xy/resolution;
	
	//warped uv
	vec2 crt_uv=2.*uv-1.;
	vec2 offset=crt_uv.yx/Curvature;
	crt_uv+=crt_uv*offset*offset;
	crt_uv=.5+.5*crt_uv;

	//blur transition
	vec2 edge=smoothstep(0., Blur, crt_uv)*(1.-smoothstep(1.-Blur, 1., crt_uv));

	//chromatic abberation
	o_frag_col=vec4(
		edge.x*edge.y*vec3(
			texture(sampler2D(b_crt_tex, b_crt_smp), .5+(crt_uv-.5)*CAScale).r,
			texture(sampler2D(b_crt_tex, b_crt_smp), crt_uv).g,
			texture(sampler2D(b_crt_tex, b_crt_smp), .5+(crt_uv-.5)/CAScale).b
		),
		1.
	);

	//scanlines
	float scan=.75*.5*abs(sin(time+gl_FragCoord.y));
	o_frag_col.rgb=mix(o_frag_col.rgb, vec3(0.), scan);
}

@end

@program crt vs_post_process fs_crt


//halftone
@fs fs_halftone

in float time;
in vec2 resolution;
out vec4 o_frag_col;

layout(binding=0) uniform texture2D b_halftone_tex;
layout(binding=0) uniform sampler b_halftone_smp;

@include_block utils

const float QSqrt2=.7071068;

const float Size=7.;
const float FPS=8.;

const float Blur=.15;

const float Low=.12;
const float High=.85;

const float Spread=.085;

vec2 rotVec(vec2 v, float f) {
	float c=cos(f), s=sin(f);
	return v*mat2(c, -s, s, c);
}

void main() {
	//randomly offsetted to avoid moire.
	float rand_seed=round(time*FPS)/FPS;
	float angle_r=2.*Pi*hash11(rand_seed);
	float angle_g=angle_r+Pi*mix(.25, .33, hash11(10.+rand_seed));
	float angle_b=angle_g+Pi*mix(.25, .33, hash11(20.+rand_seed));

	//rotate, shrink, repeat, normal. radial gradients
	vec2 coord=.5*resolution-gl_FragCoord.xy;
	vec3 dist=vec3(
		length(.5-fract(rotVec(coord, angle_r)/Size)),
		length(.5-fract(rotVec(coord, angle_g)/Size)),
		length(.5-fract(rotVec(coord, angle_b)/Size))
	);

	//sample tex
	vec2 uv=gl_FragCoord.xy/resolution;
	vec4 col=QSqrt2*texture(sampler2D(b_halftone_tex, b_halftone_smp), uv);

	//threshold the bad boy
	o_frag_col=vec4(smoothstep(Blur, -Blur, dist-col.rgb), 1.);
	o_frag_col.rgb=mix(vec3(Low), vec3(High), o_frag_col.rgb);

	//film grain?
	o_frag_col.rgb+=Spread*(.5-hash32(gl_FragCoord.xy));
}

@end

@program halftone vs_post_process fs_halftone

//voronoi crosshatch
@fs fs_crosshatch

in float time;
in vec2 resolution;
out vec4 o_frag_col;

layout(binding=0) uniform texture2D b_crosshatch_tex;
layout(binding=0) uniform sampler b_crosshatch_smp;

@include_block utils

const float FPS=8.;//4-12
const float WaveDensity=.0085;//.005-.015
const float WaveScale=7.;//5-25
const float VoronoiScale=45.;//35-85
const float LineSize=4.5;//4-20
const int NumLevels=4;//3-5

float noise(vec3 p) {
	//whole and fractional portion
	vec3 dig=floor(p);
	vec3 dec=fract(p);
	
	//smoothstep
	vec3 u=dec*dec*(3.-2.*dec);
	
	//trilinear interpolation
	float a=hash13(dig+vec3(0., 0., 0.));
	float b=hash13(dig+vec3(1., 0., 0.));
	float c=hash13(dig+vec3(0., 1., 0.));
	float d=hash13(dig+vec3(1., 1., 0.));
	float e=hash13(dig+vec3(0., 0., 1.));
	float f=hash13(dig+vec3(1., 0., 1.));
	float g=hash13(dig+vec3(0., 1., 1.));
	float h=hash13(dig+vec3(1., 1., 1.));
	float ab=mix(a, b, u.x);
	float cd=mix(c, d, u.x);
	float ef=mix(e, f, u.x);
	float gh=mix(g, h, u.x);
	float abcd=mix(ab, cd, u.y);
	float efgh=mix(ef, gh, u.y);
	return mix(abcd, efgh, u.z);
}


float fbm(vec3 p) {
	float val=0.;
	float amp=.5;
	for(int i=0; i<5; i++) {
		val+=amp*noise(p);
		p*=2.;
		amp*=.5;
	}
	return val;
}

//https://www.shadertoy.com/view/MslGD8
vec2 voronoi(vec2 x) {
	vec2 n=floor(x);
	vec2 f=x-n;

	vec3 m=vec3(8.);
	for(int j=-1; j<=1; j++) {
		for(int i=-1; i<=1; i++) {
			vec2 g=vec2(i, j);
			vec2 o=hash22(n+g);
			vec2 r=g-f+(.5+.5*sin(2.*Pi*o));
			float d=dot(r, r);
			if(d<m.x) m=vec3(d, o);
		}
	}

	return vec2(sqrt(m.x), m.y+m.z);
}

//sobel simplification: transpose x for y!
const mat3 sobel=mat3(
	vec3(-1., 0., 1.),
	vec3(-2., 0., 2.),
	vec3(-1., 0., 1.)
);

void main() {
	//truncate time for animation
	float anim=round(FPS*time)/FPS;
	
	//offset coord to look like handwriting
	vec3 coord3=vec3(WaveDensity*gl_FragCoord.xy, anim);
	vec2 fbm=vec2(
		fbm(coord3),
		fbm(coord3+vec3(17.3, 43.7, 91.1))
	);
	vec2 rand_coord=gl_FragCoord.xy+WaveScale*(.5-fbm);
	
	//sample image and get luminance
	vec2 uv=rand_coord/resolution;
	vec4 base=texture(sampler2D(b_crosshatch_tex, b_crosshatch_smp), uv);
	float lum=LUM(base.rgb);
	lum=pow(lum, .75);
	
	//posterize to determine # hatches
	int num=NumLevels-int(float(NumLevels+1)*lum);
	
	//use voronoi cell id as random seed
	vec2 v=gl_FragCoord.xy+10000.*hash21(anim);
	float rand_seed=voronoi(v/VoronoiScale).y;
	float angle=2.*Pi*hash11(rand_seed);
	
	//combine layers
	float new_lum=1.;
	for(int i=0; i<num; i++) {
		//offset angle to avoid moire
		rand_seed+=10.;
		angle+=Pi*mix(
			1./float(1+num),
			1./float(num),
			hash11(rand_seed)
		);
		
		//procedural hatch pattern
		float dist=dot(rand_coord, vec2(cos(angle), sin(angle)));
		float repeat=2.*abs(.5-fract(dist/LineSize));
		repeat=smoothstep(0., 1., repeat);
		new_lum*=repeat;
	}
	
	//add edge lines w/ sobel
	vec2 sum=vec2(0);
	for(int i=0; i<3; i++) {
		for(int j=0; j<3; j++) {
			vec2 offset_uv=vec2(i-1, j-1)/resolution;
			vec4 col=texture(sampler2D(b_crosshatch_tex, b_crosshatch_smp), uv+offset_uv);

			float lum=LUM(col.rgb);
			sum+=lum*vec2(sobel[i][j], sobel[j][i]);
		}
	}
	float mag=length(sum);
	new_lum*=1.-smoothstep(.6, .95, mag);
	
	o_frag_col=vec4(new_lum*base.rgb, 1.);
}

@end

@program crosshatch vs_post_process fs_crosshatch

//ascii text shader
@fs fs_ascii

in float time;
in vec2 resolution;
out vec4 o_frag_col;

layout(binding=0) uniform texture2D b_ascii_tex;
layout(binding=0) uniform sampler b_ascii_smp;

@include_block utils

//round 6x6 font packed into 108x 32bit integers
//https://frostyfreeze.itch.io/pixel-bitmap-fonts-png-xml
const uint font[]=uint[](
	0x00000000u, 0x60061860u, 0x00028a00u, 0x94f94000u,
	0x2384014fu, 0x42210e30u, 0x42022108u, 0x101e1421u,
	0x00000004u, 0x08104108u, 0x21041020u, 0x50850000u,
	0x3e208000u, 0x00000082u, 0x0000c200u, 0x0000003eu,
	0x00030000u, 0x00210842u, 0x0e45144eu, 0xf1041070u,
	0x05e40f01u, 0x1c41f01fu, 0xf45101f4u, 0x05f01041u,
	0x4e00f40fu, 0xf00e44f0u, 0x00411883u, 0x1c89c89cu,
	0xe83c89c0u, 0x30030001u, 0x00000000u, 0x21080000u,
	0xf8000810u, 0x02000f80u, 0xe0021181u, 0x00c00c40u,
	0x0e04d246u, 0x145f44e0u, 0x44f44f01u, 0x4144e00fu,
	0x144f00e4u, 0x05f00f45u, 0x5f01f047u, 0xe0010470u,
	0x00e45d04u, 0x2187f861u, 0xe20823e0u, 0x8a082003u,
	0x8e4a201cu, 0x20820224u, 0xda203e08u, 0xd10228aau,
	0xe0116554u, 0x00e45144u, 0x0104f44fu, 0x625544e0u,
	0x24f44f01u, 0x0e05e011u, 0x411f00f4u, 0x86100410u,
	0xa201e861u, 0x20085228u, 0x022daa8au, 0x22508522u,
	0x82085220u, 0x10843e00u, 0x0410c03eu, 0x000000c1u,
	0x10600000u, 0x00006104u, 0x00000000u, 0x00000000u,
	0x00000000u, 0xe2493800u, 0x2491c100u, 0x41380007u,
	0x938800e0u, 0x38000e24u, 0x8c00e04fu, 0x000208f0u,
	0x1c838938u, 0x196591c1u, 0x20820020u, 0x41001000u,
	0x8e482312u, 0x20820124u, 0x48000c08u, 0xc001249eu,
	0x00092491u, 0x00624918u, 0x472491c0u, 0xe2493800u,
	0x0491c020u, 0x03380001u, 0x21c20073u, 0x64000608u,
	0x80006659u, 0x000c3124u, 0x01279248u, 0x1230c480u,
	0x07124800u, 0x19878039u, 0x0610c01eu, 0x000000c1u,
	0x10600000u, 0x0000610cu, 0x00000000u, 0x00000000u
);

//sorted characters by luminance
const int palette[]=int[](
	32, 126, 124, 96, 95, 94, 92, 127, 
	59, 46, 39, 44, 34, 58, 105, 41, 
	42, 45, 47, 40, 60, 108, 62, 114, 
	106, 37, 93, 91, 89, 33, 125, 123, 
	120, 118, 116, 111, 63, 99, 74, 84, 
	43, 76, 86, 110, 88, 102, 119, 115, 
	109, 107, 38, 61, 97, 100, 70, 75, 
	49, 121, 52, 117, 113, 112, 55, 67, 
	36, 101, 98, 64, 80, 103, 85, 79, 
	122, 48, 104, 90, 56, 87, 71, 83, 
	54, 81, 78, 77, 73, 65, 57, 68, 
	82, 72, 53, 69, 51, 50, 35, 66
);

void main() {
	//truncate coordinates
	vec2 coord=6.*(.5+floor(gl_FragCoord.xy/6.));
	vec2 uv=coord/resolution;
	
	//sample col & get luminance
	vec4 col=texture(sampler2D(b_ascii_tex, b_ascii_smp), uv);
	float lum=LUM(col.rgb);
	
	//find said character from palette
	int pal_ix=int(float(palette.length())*lum);
	pal_ix=clamp(pal_ix, 0, palette.length()-1);
	int ltr=palette[pal_ix];
	
	//repeating character int coords
	int x=int(gl_FragCoord.x)%6;
	int y=5-int(gl_FragCoord .y)%6;
	
	//find packed bit position, said word, and mask out bit
	int bit_ix=x+6*y+36*(ltr-32);
	int word_ix=bit_ix/32;
	uint word=font[word_ix];
	bool bit=1u==(1u&(word>>(bit_ix%32)));
	
	//use "saturated" color
	float sat=max(col.r, max(col.g, col.b));
	if(bit) o_frag_col.rgb=col.rgb/sat;
	else o_frag_col.rgb=vec3(0);
	
	o_frag_col.a=1.;
}

@end

@program ascii vs_post_process fs_ascii

//kuwahara
@fs fs_kuwahara

in float time;
in vec2 resolution;
out vec4 o_frag_col;

layout(binding=0) uniform texture2D b_kuwahara_tex;
layout(binding=0) uniform sampler b_kuwahara_smp;

@include_block utils

const int K=5;//3-7

#define KERN_TEX(a, b)\
texture(sampler2D(b_kuwahara_tex, b_kuwahara_smp), (gl_FragCoord.xy+vec2(i-a, j-b))/resolution).rgb

void main() {
	float sector_sz=float(1+2*K+K*K);
	
	//compute 4 sector means
	vec3 u0=vec3(0.), u1=u0, u2=u0, u3=u0;
	for(int i=0; i<=K; i++) {
		for(int j=0; j<=K; j++) {
			u0+=KERN_TEX(K, K);
			u1+=KERN_TEX(0, K);
			u2+=KERN_TEX(K, 0);
			u3+=KERN_TEX(0, 0);
		}
	}
	u0/=sector_sz, u1/=sector_sz, u2/=sector_sz, u3/=sector_sz;
	vec4 u=vec4(LUM(u0), LUM(u1), LUM(u2), LUM(u3));
	
	//compute those sector's variance
	vec4 var=vec4(0.);
	for(int i=0; i<=K; i++) {
		for(int j=0; j<=K; j++) {
			vec4 v=vec4(
				LUM(KERN_TEX(K, K)),
				LUM(KERN_TEX(0, K)),
				LUM(KERN_TEX(K, 0)),
				LUM(KERN_TEX(0, 0))
			)-u;
			var+=v*v;
		}
	}
	
	//set pix to mean of sector with min variance
	float m=min(var.x, min(var.y, min(var.z, var.w)));
	o_frag_col=vec4(
		m==var.x?u0:
		m==var.y?u1: 
		m==var.z?u2:
		m==var.w?u3:
		.25*(u0+u1+u2+u3),
		1.
	);
}

@end

@program kuwahara vs_post_process fs_kuwahara

//mean of least variance
@fs fs_mlv

in float time;
in vec2 resolution;
out vec4 o_frag_col;

layout(binding=0) uniform texture2D b_mlv_tex;
layout(binding=0) uniform sampler b_mlv_smp;

@include_block utils

const int K=5;//3-7

//stipple
const int bayer[16]=int[16](
	0, 8, 2, 10,
	12, 4, 14, 6,
	3, 11, 1, 9,
	15, 7, 13, 5
);

const int NumLevels=4;

void main() {
	vec2 uv=gl_FragCoord.xy/resolution;
	
	//optimizations?
	float num=float(K*K);
	float n_weight=1./num;
	float n1_weight=1./(num-1.);
	
	//every possible kernel
	float min_var=0.;
	for(int oi=0; oi<K; oi++) {
		for(int oj=0; oj<K; oj++) {
			int ox=oi-K+1, oy=oj-K+1;
			
			//naive variance formula
			vec4 sum=vec4(0.), sum_sq=sum;
			for(int ii=0; ii<K; ii++) {
				for(int ij=0; ij<K; ij++) {
					vec2 offset=vec2(ox+ii, oy+ij)/resolution;
					vec4 col=texture(sampler2D(b_mlv_tex, b_mlv_smp), uv+offset);
					vec4 value=vec4(col.rgb, LUM(col.rgb));
					sum+=value;
					sum_sq+=value*value;
				}
			}
			float var=n1_weight*(sum_sq.a-n_weight*sum.a*sum.a);
			
			//"sort" while sampling
			if((oi==0&&oj==0)||var<min_var) {
				min_var=var;
				o_frag_col=vec4(n_weight*sum.rgb, 1);
			}
		}
	}
	
	//dithering
	ivec2 ij=ivec2(gl_FragCoord.xy)%4;
	float bayer_value=.0625*float(bayer[ij.x+4*ij.y]);
	float mask=.5-bayer_value;
	o_frag_col.rgb+=.23*mask;
	
	//posterization
	float N1=float(NumLevels-1);
	o_frag_col.rgb=floor(.5+N1*o_frag_col.rgb)/N1;
}

@end

@program mlv vs_post_process fs_mlv