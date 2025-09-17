#pragma once
#ifndef ARM_CLASS_H
#define ARM_CLASS_H
struct Point {
	vf2d pos;
	bool locked=false;

	static void constrain(Point& a, Point& b, float len) {
		vf2d sub=a.pos-b.pos;
		float curr=sub.mag();

		//dont divide by 0
		vf2d norm;
		if(curr<1e-6f) norm=cmn::polar<vf2d>(1, cmn::randFloat(2*cmn::Pi));
		else norm=sub/curr;

		//find change
		float delta=len-curr;
		vf2d diff=.5f*delta*norm;

		//update
		if(!a.locked) a.pos+=diff;
		if(!b.locked) b.pos-=diff;
	}
};

class Arm {
	int num=0;

	void copyFrom(const Arm&), clear();

public:
	Point* pts=nullptr;
	float len=0;
	float hold_rad=0;
	vf2d hold_pos;

	Arm() {}

	Arm(int n, vf2d s, vf2d e, float l, float h) {
		num=n;
		pts=new Point[num];
		len=l;

		pts[0].pos=s;
		pts[0].locked=true;

		hold_rad=h;
		hold_pos=e;
		pts[num-1].locked=true;
	}

	//1
	Arm(const Arm& a) {
		copyFrom(a);
	}

	//2
	~Arm() {
		clear();
	}

	//3
	Arm& operator=(const Arm& a) {
		if(&a==this) return *this;

		clear();

		copyFrom(a);

		return *this;
	}

	int getNum() const {
		return num;
	}

	void update(vf2d new_hold) {
		//anchors
		if((new_hold-hold_pos).mag()>hold_rad) hold_pos=new_hold;
		pts[num-1].pos=hold_pos;

		//segments
		for(int i=1; i<num; i++) {
			Point::constrain(pts[i-1], pts[i], len);
		}
	}
};

void Arm::copyFrom(const Arm& a) {
	num=a.num;
	pts=new Point[num];
	for(int i=0; i<num; i++) {
		pts[i]=a.pts[i];
	}
	len=a.len;
	hold_rad=a.hold_rad;
	hold_pos=a.hold_pos;
}

void Arm::clear() {
	delete[] pts;
}

#endif