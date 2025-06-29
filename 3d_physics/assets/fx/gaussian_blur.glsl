const float Pi=3.1415927;

const int K=7;
const float Sigma=.71;

//technically the "left" coefficient doesnt matter
//	as it doesn't depend on pos, so it cancels out
float gaussian(vec2 pos) {
	float left=1/(2*Pi*Sigma*Sigma);
	float right=exp(-dot(pos, pos)/(2*Sigma*Sigma));
	return left*right;
}

void main() {
	vec2 resolution=textureSize(tex1, 0);
	
	//apply convolution
	vec4 sum=vec4(0);
	for(int i=-K; i<=K; i++) {
		for(int j=-K; j<=K; j++) {
			vec2 offset=vec2(i, j);
			float weight=gaussian(offset);
			
			vec2 offset_uv=offset/resolution;
			vec4 col=texture(tex1, xUV1+offset_uv);
			sum+=weight*vec4(col.rgb, 1);
		}
	}
	pix_out=sum/sum.a;
}