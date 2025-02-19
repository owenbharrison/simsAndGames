#pragma once
#ifndef GEOM_CONSTRAINT_H
#define GEOM_CONSTRAINT_H

#include "point_constraint.h"

#include <cmath>

static constexpr float Epsilon=1e-6f;

//rotation matrix
vf2d rotate(const vf2d& v, float f) {
	float c=std::cosf(f), s=std::sinf(f);
	return {
		v.x*c-v.y*s,
		v.x*s+v.y*c
	};
}

struct GeometryConstraint {
protected:
	int id=0;

public:
	LineConstraint* a=nullptr, * b=nullptr;

	int getID() const { return id; }

	bool operator==(const GeometryConstraint& other) const {
		//two constraints can connect two points
		if(id!=other.id) return false;

		//same points? or reversed?
		if(a==other.a&&b==other.b) return true;
		if(a==other.b&&b==other.a) return true;

		return false;
	}

	virtual GeometryConstraint* clone() const=0;

	virtual void solve()=0;
};

#define EQUAL_CST 1
struct EqualConstraint : GeometryConstraint {
	EqualConstraint(LineConstraint* a_, LineConstraint* b_) { id=EQUAL_CST, a=a_, b=b_; }

	EqualConstraint* clone() const override {
		return new EqualConstraint(*this);
	}

	void solve() override {
		float& len_a=a->length;
		float& len_b=b->length;
		float mid=(len_a+len_b)/2;
		len_a=len_b=mid;
	}
};

#define ANGLE_CST 2
struct AngleConstraint : GeometryConstraint {
	float angle=0;

	AngleConstraint(LineConstraint* a_, LineConstraint* b_) {
		id=ANGLE_CST, a=a_, b=b_;
		update();
	}

	AngleConstraint* clone() const override {
		return new AngleConstraint(*this);
	}

	void update() {
		//sort points

		vf2d ab=*a->b-*a->a;
		vf2d cd=*b->b-*b->a;
		float denom=ab.mag()*cd.mag();
		if(denom<Epsilon) return;
		float curr=std::acosf(ab.dot(cd)/denom);

		//convert to degrees later.
		angle=std::acosf(ab.dot(cd)/denom);
	}

	void solve() override {
		vf2d& p_a=*a->a;
		vf2d& p_b=*a->b;
		vf2d& p_c=*b->a;
		vf2d& p_d=*b->b;
		//get current angle
		vf2d ab=p_b-p_a;
		vf2d cd=p_d-p_c;
		float denom=ab.mag()*cd.mag();
		if(denom<Epsilon) return;
		float curr=std::acosf(ab.dot(cd)/denom);

		//where do they need to be?
		float diff=(angle-curr)/2;

		//rot a,b about their mid
		vf2d m_ab=(p_a+p_b)/2;
		p_a=m_ab+rotate(p_a-m_ab, -diff);
		p_b=m_ab+rotate(p_b-m_ab, -diff);

		//rot c,d about their mid
		vf2d m_cd=(p_c+p_d)/2;
		p_c=m_cd+rotate(p_c-m_cd, diff);
		p_d=m_cd+rotate(p_d-m_cd, diff);
	}
};
#endif//GEOM_CONSTRAINT_H