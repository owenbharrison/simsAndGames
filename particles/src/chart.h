#pragma once
#ifndef CHART_CLASS_H
#define CHART_CLASS_H
class Chart {
	int max_num=0;
	int num=0;

	void copyFrom(const Chart&);
	void clear();

public:
	float* values=nullptr;

	cmn::AABB bounds;

	Chart() {}

	Chart(int m, const cmn::AABB& b) : max_num(m) {
		values=new float[max_num];
		bounds=b;
	}

	Chart(const Chart& c) {
		copyFrom(c);
	}

	~Chart() {
		clear();
	}

	Chart& operator=(const Chart& c) {
		if(&c==this) return *this;

		clear();

		copyFrom(c);

		return *this;
	}

	int getMaxNum() const {
		return max_num;
	}

	int getNum() const {
		return num;
	}

	//only change how many values
	void reset() {
		num=0;
	}

	void update(float value) {
		//increment if not full
		if(num<max_num) num++;
		else {
			//shift values if full
			for(int i=0; i<max_num-1; i++) {
				values[i]=values[i+1];
			}
		}
		//add new value at end
		values[num-1]=value;
	}

	float getMinValue() {
		float min=0;
		for(int i=0; i<num; i++) {
			if(i==0||values[i]<min) {
				min=values[i];
			}
		}
		return min;
	}

	float getMaxValue() {
		float max=0;
		for(int i=0; i<num; i++) {
			if(i==0||values[i]>max) {
				max=values[i];
			}
		}
		return max;
	}
};

void Chart::copyFrom(const Chart& c) {
	max_num=c.max_num;
	values=new float[max_num];

	//copy over necessary values
	num=c.getNum();
	for(int i=0; i<num; i++) {
		values[i]=c.values[i];
	}

	//copy over box
	bounds=c.bounds;
}

void Chart::clear() {
	delete[] values;
}
#endif