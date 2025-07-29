#pragma once
#ifndef PACKING_UTILS_H
#define PACKING_UTILS_H
//0xAABBCCDD
int pack(unsigned char a, unsigned char b, unsigned char c, unsigned char d) {
	return (a<<24)|(b<<16)|(c<<8)|d;
}

void unpack(int i, unsigned char& a, unsigned char& b, unsigned char& c, unsigned char& d) {
	a=0xff&(i>>24);
	b=0xff&(i>>16);
	c=0xff&(i>>8);
	d=0xff&(i>>0);
}
#endif