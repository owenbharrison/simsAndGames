#pragma once
#ifndef POINT_CONSTRAINT_H
#define POINT_CONSTRAINT_H

struct PointConstraint {
protected:
	int id=0;

public:
	vf2d* a=nullptr, * b=nullptr;

	int getID() const { return id; }

	bool operator==(const PointConstraint& other) const {
		//two constraints can connect two points
		if(id!=other.id) return false;

		//same points? or reversed?
		if(a==other.a&&b==other.b) return true;
		if(a==other.b&&b==other.a) return true;

		return false;
	}
	
	virtual PointConstraint* clone() const=0;

	virtual void solve()=0;
};

#define VERT_CST 1
struct VerticalConstraint : public PointConstraint {
	VerticalConstraint(vf2d* a_, vf2d* b_) { id=VERT_CST, a=a_, b=b_; }

	VerticalConstraint* clone() const override {
		return new VerticalConstraint(*this);
	}

	void solve() override {
		float mid=(a->x+b->x)/2;
		a->x=b->x=mid;
	}
};

#define HORIZ_CST 2
struct HorizontalConstraint : PointConstraint {
	HorizontalConstraint(vf2d* a_, vf2d* b_) { id=HORIZ_CST, a=a_, b=b_; }

	HorizontalConstraint* clone() const override {
		return new HorizontalConstraint(*this);
	}

	void solve() override {
		float mid=(a->y+b->y)/2;
		a->y=b->y=mid;
	}
};

#define LINE_CST 3
struct LineConstraint : public PointConstraint {
	float length=0;

	LineConstraint(vf2d* a_, vf2d* b_) {
		id=LINE_CST, a=a_, b=b_;
		update();
	}

	LineConstraint* clone() const override {
		return new LineConstraint(*this);
	}

	void update() {
		length=(*a-*b).mag();
	}

	void solve() override {
		static const float epsilon=1e-6f;
		vf2d sub=*a-*b;
		float mag=sub.mag();
		if(mag<epsilon) return;

		vf2d norm=sub/mag;
		float diff=(length-mag)/2;
		*a+=diff*norm;
		*b-=diff*norm;
	}
};
#endif//POINT_CONSTRAINT_H