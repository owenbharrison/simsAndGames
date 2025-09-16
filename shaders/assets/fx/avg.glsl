void main() {
	//shrink uv and translate to left & right
	vec2 uv1=vec2(.5*xUV1.x, xUV1.y);
	vec2 uv2=vec2(.5+.5*xUV1.x, xUV1.y);

	//get both colors
	vec4 col1=texture(tex1, uv1);
	vec4 col2=texture(tex1, uv2);

	//average
	pix_out=(col1+col2)/2;
}