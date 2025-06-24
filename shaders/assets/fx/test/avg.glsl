void main() {
	vec4 col1=texture(tex1, xUV1);
	vec4 col2=texture(tex2, xUV2);
	pix_out=(col1+col2)/2;
}