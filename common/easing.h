//adapted from https://easings.net
#pragma once

#ifndef CMN_EASING_UTIL_H
#define CMN_EASING_UTIL_H

#include <cmath>

namespace cmn {
	class ease {
		static const float pi, c1, c2, c3, c4, c5;

	public:
		static float linear(float x) {
			return x;
		}

		static float inQuad(float x) {
			return x*x;
		}

		static float outQuad(float x) {
			return 1-(1-x)*(1-x);
		}

		static float inOutQuad(float x) {
			return x<.5f?2*x*x:1-std::pow(-2*x+2, 2)/2;
		}

		static float inCubic(float x) {
			return x*x*x;
		}

		static float outCubic(float x) {
			return 1-std::pow(1-x, 3);
		}

		static float inOutCubic(float x) {
			return x<.5f?4*x*x*x:1-std::pow(-2*x+2, 3)/2;
		}

		static float inQuart(float x) {
			return x*x*x*x;
		}

		static float outQuart(float x) {
			return 1-std::pow(1-x, 4);
		}

		static float inOutQuart(float x) {
			return x<.5f?8*x*x*x*x:1-std::pow(-2*x+2, 4)/2;
		}

		static float inQuint(float x) {
			return x*x*x*x*x;
		}

		static float outQuint(float x) {
			return 1-std::pow(1-x, 5);
		}

		static float inOutQuint(float x) {
			return x<.5f?16*x*x*x*x*x:1-std::pow(-2*x+2, 5)/2;
		}

		static float inSine(float x) {
			return 1-std::cos((x*pi)/2);
		}

		static float outSine(float x) {
			return std::sin((x*pi)/2);
		}

		static float inOutSine(float x) {
			return -(std::cos(pi*x)-1)/2;
		}

		static float inExpo(float x) {
			return x==0?0:std::pow(2, 10*x-10);
		}

		static float outExpo(float x) {
			return x==1?1:1-std::pow(2, -10*x);
		}

		static float inOutExpo(float x) {
			return x==0
				?0
				:x==1
				?1
				:x<.5f
				?std::pow(2, 20*x-10)/2
				:(2-std::pow(2, -20*x+10))/2;
		}

		static float inCirc(float x) {
			return 1-std::sqrt(1-std::pow(x, 2));
		}

		static float outCirc(float x) {
			return std::sqrt(1-std::pow(x-1, 2));
		}

		static float inOutCirc(float x) {
			return x<.5f
				?(1-std::sqrt(1-std::pow(2*x, 2)))/2
				:(std::sqrt(1-std::pow(-2*x+2, 2))+1)/2;
		}

		static float inBack(float x) {
			return c3*x*x*x-c1*x*x;
		}

		static float outBack(float x) {
			return 1+c3*std::pow(x-1, 3)+c1*std::pow(x-1, 2);
		}

		static float inOutBack(float x) {
			return x<.5f
				?(std::pow(2*x, 2)*((c2+1)*2*x-c2))/2
				:(std::pow(2*x-2, 2)*((c2+1)*(x*2-2)+c2)+2)/2;
		}

		static float inElastic(float x) {
			return x==0
				?0
				:x==1
				?1
				:-std::pow(2, 10*x-10)*std::sin((x*10-10.75f)*c4);
		}

		static float outElastic(float x) {
			return x==0
				?0
				:x==1
				?1
				:std::pow(2, -10*x)*std::sin((x*10-.75f)*c4)+1;
		}

		static float inOutElastic(float x) {
			return x==0
				?0
				:x==1
				?1
				:x<.5f
				?-(std::pow(2, 20*x-10)*std::sin((20*x-11.125f)*c5))/2
				:(std::pow(2, -20*x+10)*std::sin((20*x-11.125f)*c5))/2+1;
		}

		static float inBounce(float x) {
			return 1-bounceOut(1-x);
		}

		static float bounceOut(float x) {
			const float n1=7.5625f;
			const float d1=2.75f;

			if(x<1/d1) {
				return n1*x*x;
			} else if(x<2/d1) {
				float t=x-1.5f/d1;
				return n1*t*t+.75f;
			} else if(x<2.5f/d1) {
				float t=x-2.25f/d1;
				return n1*t*t+.9375f;
			} else {
				float t=x-2.625f/d1;
				return n1*t*t+.984375f;
			}
		}

		static float inOutBounce(float x) {
			return x<.5f
				?(1-bounceOut(1-2*x))/2
				:(1+bounceOut(2*x-1))/2;
		}
	};

	const float ease::pi=3.1415927f;
	const float ease::c1=1.70158f;
	const float ease::c2=c1*1.525f;
	const float ease::c3=c1+1;
	const float ease::c4=(2*pi)/3;
	const float ease::c5=(2*pi)/4.5f;
}
#endif