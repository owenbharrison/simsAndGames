//integer packing header
//assuming scene only has 256 unique spritesheets or objects...
//could also use an id->ss_idx lookup...
//int layout:
//0xAABBCCDD

#pragma once
#ifndef PACKING_UTIL_H
#define PACKING_UTIL_H

//pack four uchars into an int
int pack(unsigned char a, unsigned char b, unsigned char c, unsigned char d) {
	return (a<<24)|(b<<16)|(c<<8)|d;
}

//retrieve four uchars from an int
void unpack(int packed, unsigned char& a, unsigned char& b, unsigned char& c, unsigned char& d) {
	a=0xFF&(packed>>24);
	b=0xFF&(packed>>16);
	c=0xFF&(packed>>8);
	d=0xFF&packed;
}
#endif