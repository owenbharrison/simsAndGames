#define K 5//[3-7]

//luminance
#define GRAY(a) dot(a, vec3(.299, .587, .114))

//stipple
const int bayer[16]=int[16](
	0, 8, 2, 10,
	12, 4, 14, 6,
	3, 11, 1, 9,
	15, 7, 13, 5
);

const uint PostNum=4u;

void main() {
	vec2 resolution=textureSize(tex1, 0);
	
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
			vec4 sum=vec4(0), sum_sq=sum;
			for(int ii=0; ii<K; ii++) {
				for(int ij=0; ij<K; ij++) {
					vec2 offset_uv=vec2(ox+ii, oy+ij)/resolution;
					vec3 col=texture(tex1, xUV1+offset_uv).rgb;
					vec4 value=vec4(col, GRAY(col));
					sum+=value;
					sum_sq+=value*value;
				}
			}
			float var=n1_weight*(sum_sq.a-n_weight*sum.a*sum.a);
			
			//"sort" while sampling
			if((oi==0&&oj==0)||var<min_var){
				min_var=var;
				pix_out=vec4(n_weight*sum.rgb, 1);
			}
		}
	}
	
	//dithering
	vec2 coord=resolution*xUV1;
	ivec2 ij=ivec2(coord)%4;
	float bayer_val=.0625*float(bayer[ij.x+4*ij.y]);
	float mask=.5-bayer_val;
	pix_out.rgb+=.23*mask;
	
	//posterization
	float n1=float(PostNum-1u);
	pix_out.rgb=floor(.5+n1*pix_out.rgb)/n1;
}