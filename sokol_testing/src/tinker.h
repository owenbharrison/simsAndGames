//c++ version of adjust.h
//inspired by https://github.com/bi3mer/adjust.h
#pragma once
#ifndef TINKER_UTIL_H
#define TINKER_UTIL_H

#include <fstream>
#include <string>

//hot reload macro
#define TINKER_FLOAT(_, x) {\
	std::ifstream _file(__FILE__);\
	std::string _line;\
	for(int i=1; i<=__LINE__; i++) std::getline(_file, _line);\
	static const std::string _word="TINKER_FLOAT";\
	int _st_a=0;\
	for(; _st_a<=_line.length()-_word.length(); _st_a++) {\
		auto _sub=_line.substr(_st_a, _word.length());\
		if(_sub==_word) break;\
	}\
	_st_a+=1+_word.length();\
	int _end_a=1+_st_a;\
	for(; _end_a<_line.length(); _end_a++) {\
		if(_line[_end_a]==',') break;\
	}\
	x=std::stof(_line.substr(_st_a, _end_a-_st_a));\
}

#define TINKER_INT(_, x) {\
	std::ifstream _file(__FILE__);\
	std::string _line;\
	for(int i=1; i<=__LINE__; i++) std::getline(_file, _line);\
	static const std::string _word="TINKER_INT";\
	int _st_a=0;\
	for(; _st_a<=_line.length()-_word.length(); _st_a++) {\
		auto _sub=_line.substr(_st_a, _word.length());\
		if(_sub==_word) break;\
	}\
	_st_a+=1+_word.length();\
	int _end_a=1+_st_a;\
	for(; _end_a<_line.length(); _end_a++) {\
		if(_line[_end_a]==',') break;\
	}\
	x=std::stoi(_line.substr(_st_a, _end_a-_st_a));\
}
#endif