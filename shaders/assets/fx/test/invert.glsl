void main() {
	vec4 col=texture(tex1, xUV1);
	pix_out=vec4(1-col.rgb, 1);
}