#pragma once
#ifndef LINE_CONSTRAINT_H
#define LINE_CONSTRAINT_H

#include "point_constraint.h"

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
class EqualConstraint : public GeometryConstraint {
	EqualConstraint();

public:
	EqualConstraint(LineConstraint* a_, LineConstraint* b_) { id=EQUAL_CST, a=a_, b=b_; }

	EqualConstraint* clone() const override {
		return new EqualConstraint(*this);
	}

	void solve() override {
		float& len_a=a->curr;
		float& len_b=b->curr;
		float mid=(len_a+len_b)/2;
		len_a=len_b=mid;
	}
};
#endif//LINE_CONSTRAINT_H