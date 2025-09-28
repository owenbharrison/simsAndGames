//SPACE thru DEL
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
	//pixel space coordinates
	vec2 resolution=textureSize(tex1, 0);
	vec2 coord=resolution*xUV1;

	//truncate coordinates
	vec2 trunc_coord=6*floor(coord/6);
	vec2 trunc_uv=coord/resolution;

	//sample col & get luminance
	vec4 col=texture(tex1, trunc_uv);
	float lum=dot(vec3(.299, .587, .114), col.rgb);
	
	//find said character from palette
	int pal_ix=int(float(palette.length())*lum);
	pal_ix=clamp(pal_ix, 0, palette.length()-1);
	int ltr=palette[pal_ix];
	
	//repeating character int coords
	int x=int(coord.x)%6;
	int y=5-int(coord.y)%6;
	
	//find packed bit position, said word, and mask out bit
	int bit_ix=x+6*y+36*(ltr-32);
	int word_ix=bit_ix/32;
	uint word=font[word_ix];
	bool bit=1u==(1u&(word>>(bit_ix%32)));
	
	//use "saturated" color
	float sat=max(col.r, max(col.g, col.b));
	if(bit) pix_out.rgb=col.rgb/sat;
	else pix_out.rgb=vec3(0);
	pix_out.a=1;
}