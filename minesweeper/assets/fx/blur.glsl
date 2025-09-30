//gaussian blur
#define K 10

#define PI 3.1415927

#define SIGMA 4.3
float gaussian(vec2 pos) {
    float left=1/(2*PI*SIGMA*SIGMA);
    float right=exp(-dot(pos, pos)/(2*SIGMA*SIGMA));
    return left*right;
}

void main() {
	vec2 resolution=textureSize(tex1, 0);

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