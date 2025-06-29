#pragma once
#ifndef SHADER_CLASS_H
#define SHADER_CLASS_H

#include <exception>

class Shader {
	void copyFrom(const Shader&), clear();

public:
	olc::Sprite* logo_spr=nullptr;
	olc::Decal* logo_dec=nullptr;
	std::list<olc::Effect> passes;

	Shader() {}

	Shader(olc::Shade& s, const std::string& l, const std::vector<std::string>& efs) {
		//setup logo sprite and decal
		logo_spr=new olc::Sprite(l);
		logo_dec=new olc::Decal(logo_spr);
		
		//load each shader pass
		for(const auto& ef:efs) {
			olc::Effect e=s.MakeEffect(loadEffect(ef));
			if(!e.IsOK()) throw std::runtime_error(e.GetStatus());

			passes.push_back(e);
		}
	}

	//1
	Shader(const Shader& s) {
		copyFrom(s);
	}

	//2
	~Shader() {
		clear();
	}
	
	//3
	Shader& operator=(const Shader& s) {
		if(&s==this) return *this;
		
		clear();

		copyFrom(s);

		return *this;
	}

	static olc::EffectConfig loadEffect(const std::string& filename) {
		//get file
		std::ifstream file(filename);
		if(file.fail()) throw std::runtime_error("invalid filename: "+filename);

		//dump contents into str stream
		std::stringstream mid;
		mid<<file.rdbuf();

		return {
			DEFAULT_VS,
			mid.str(),
			1,
			1
		};
	}
};

void Shader::copyFrom(const Shader& s) {
	//copy over logo sprite data
	logo_spr=new olc::Sprite(s.logo_spr->width, s.logo_spr->height);
	logo_spr->pColData=s.logo_spr->pColData;
	logo_dec=new olc::Decal(logo_spr);

	passes=s.passes;
}

void Shader::clear() {
	delete logo_dec;
	delete logo_spr;
}
#endif