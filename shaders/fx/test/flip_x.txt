void main() {
	vec2 uv=vec2(1-xUV1.x, xUV1.y);
	pix_out=texture(tex1, uv);
}