#pragma once

#ifndef DELAUNAY_UTIL_H
#define DELAUNAY_UTIL_H

//for sort
#include <algorithm>
#include <array>
#include <unordered_set>
#include <vector>

using Tri=std::array<int, 3>;
using Edge=std::array<int, 2>;

//also signed area
float cross(const cmn::vf2d& a, const cmn::vf2d& b, const cmn::vf2d& c) {
	return (b.x-a.x)*(c.y-a.y)-(b.y-a.y)*(c.x-a.x);
}

bool in_circumcircle(const cmn::vf2d& p,
	const cmn::vf2d& v1, const cmn::vf2d& v2, const cmn::vf2d& v3) {
	cmn::vf2d a=v1-p, b=v2-p, c=v3-p;
	float a_sq=a.mag_sq(), b_sq=b.dot(b), c_sq=c.dot(c);
	return
		a.x*(b.y*c_sq-c.y*b_sq)
		-a.y*(b.x*c_sq-c.x*b_sq)
		+a_sq*(b.x*c.y-b.y*c.x)>0;
}

//not including endpoints.
bool segments_intersect(
	const cmn::vf2d& p1, const cmn::vf2d& p2,
	const cmn::vf2d& p3, const cmn::vf2d& p4) {
	float d1=cross(p1, p2, p3);
	float d2=cross(p1, p2, p4);
	float d3=cross(p3, p4, p1);
	float d4=cross(p3, p4, p2);
	return d1*d2<0&&d3*d4<0;
}

Edge make_edge(int a, int b) {
	return (a<b)?Edge{a, b}:Edge{b, a};
}

template<typename T>
void add_unique(std::vector<T>& vec, T& val) {
	auto it=std::find(vec.begin(), vec.end(), val);
	if(it==vec.end()) vec.push_back(val);
}

//in struct form? wtv.
struct CDT {
	std::vector<cmn::vf2d> verts;
	std::vector<Edge> constraints;
	std::vector<Tri> tris;

	void triangulate() {
		if(verts.empty()) return;

		//(n, n+1, n+2)
		int n=verts.size();
		build_super_triangle();

		for(int i=0; i<n; i++) bowyer_watson_insert(i);

		remove_super_triangle(n);

		for(const auto& e:constraints) insert_constraint_edge(e[0], e[1]);
	}

	void build_super_triangle() {
		//bounding box
		cmn::vf2d min(1e30f, 1e30f);
		cmn::vf2d max(-1e30f, -1e30f);
		for(const auto& p:verts) {
			if(p.x<min.x) min.x=p.x;
			if(p.y<min.y) min.y=p.y;
			if(p.x>max.x) max.x=p.x;
			if(p.y>max.y) max.y=p.y;
		}

		//tri containing everything
		cmn::vf2d sz=max-min;
		verts.push_back({min.x-2*sz.x, min.y-2*sz.y});
		verts.push_back({max.x+2*sz.x, min.y-2*sz.y});
		verts.push_back({min.x+.5f*sz.x, max.y+2*sz.y});
		int sv=verts.size();
		tris.push_back({sv-3, sv-2, sv-1});
	}

	void remove_super_triangle(int n) {
		tris.erase(std::remove_if(tris.begin(), tris.end(),
			[n] (const Tri& t) {
			return t[0]>=n||t[1]>=n||t[2]>=n;
		}), tris.end());

		verts.resize(n);
	}

	void bowyer_watson_insert(int pidx) {
		const auto& p=verts[pidx];

		//find all tris whose circumcircle contains p
		std::vector<Tri> bad;
		std::vector<bool> is_bad(tris.size(), false);
		for(size_t i=0; i<tris.size(); i++) {
			const Tri& t=tris[i];
			if(in_circumcircle(p, verts[t[0]], verts[t[1]], verts[t[2]])) {
				is_bad[i]=true;
				bad.push_back(t);
			}
		}

		//find the boundary of the polygonal hole
		//i.e. edges shared by one bad tri
		std::vector<Edge> boundary;
		for(auto& t:bad) {
			for(int e=0; e<3; e++) {
				Edge edge=make_edge(t[e], t[(e+1)%3]);
				//is this edge shared with another bad tri?
				bool shared=false;
				for(auto& t2:bad) {
					if(&t==&t2) continue;
					for(int f=0; f<3; f++) {
						if(make_edge(t2[f], t2[(f+1)%3])==edge) {
							shared=true;
							break;
						}
					}
					if(shared) break;
				}
				if(!shared) boundary.push_back(edge);
			}
		}

		//remove bad tris
		tris.erase(std::remove_if(tris.begin(), tris.end(),
			[&] (const Tri& t) {
			return is_bad[&t-&tris[0]];
		}), tris.end());

		//re-triangulate the hole
		for(auto& e:boundary) tris.push_back(make_ccw(e[0], e[1], pidx));
	}

	//ensure triangle vertices are ccw
	Tri make_ccw(int a, int b, int c) {
		if(cross(verts[a], verts[b], verts[c])<0) std::swap(b, c);
		return {a, b, c};
	}

	void insert_constraint_edge(int u, int v) {
		if(edge_exists(u, v)) return;

		//collect all tris whose interiors are crossed by segment uv
		//and upper & lower polygon boundary vertexes
		std::vector<int> upper_poly, lower_poly;
		std::vector<int> crossed_tris;
		find_intersecting_triangles(u, v, crossed_tris, upper_poly, lower_poly);

		//remove crossed tris
		std::unordered_set<int> remove_set(crossed_tris.begin(), crossed_tris.end());
		std::vector<Tri> kept;
		kept.reserve(tris.size()-crossed_tris.size());
		for(size_t i=0; i<tris.size(); i++) {
			if(!remove_set.count(i)) kept.push_back(tris[i]);
		}
		tris=std::move(kept);

		retriangulate_polygon(u, v, upper_poly);
		retriangulate_polygon(v, u, lower_poly);
	}

	bool edge_exists(int u, int v) const {
		for(auto& t:tris) {
			for(int i=0; i<3; i++) {
				if(t[i]==u&&t[(i+1)%3]==v) return true;
				else if(t[i]==v&&t[(i+1)%3]==u) return true;
			}
		}
		return false;
	}

	//walk from u to v
	//collecting tris crossed by segment uv and the
	//upper & lower polygon boundary vertexes
	void find_intersecting_triangles(int u, int v,
		std::vector<int>& crossed,
		std::vector<int>& upper,
		std::vector<int>& lower) {
		const auto& pu=verts[u], & pv=verts[v];

		//find tri incident to u whose interior is crossed by uv
		int cur_tri=find_start_triangle(u, v);
		if(cur_tri<0) return;

		//walk until we reach a tri incident to v
		//previous exit edge
		int peea=-1, peeb=-1;

		//two polygon chains: start at u
		upper.clear(), lower.clear();

		//iterative crossing walk
		std::unordered_set<int> visited;
		while(cur_tri>=0) {
			//safety guard
			if(visited.count(cur_tri)) break;

			visited.insert(cur_tri);
			crossed.push_back(cur_tri);

			const Tri& t=tris[cur_tri];

			//find which edge of this tri segment uv exits thru
			int next_tri=-1;
			for(int e=0; e<3; e++) {
				int a=t[e], b=t[(e+1)%3], c=t[(e+2)%3];

				//skip the entry edge
				if((a==peea&&b==peeb)||(b==peea&&a==peeb)) continue;

				//if this tri incident to v, we're done
				if(a==v||b==v||c==v) {
					//collect boundary vertexes from this tri
					for(int k=0; k<3; k++) {
						int vi=t[k];
						if(vi==u||vi==v) continue;

						float side=cross(pu, pv, verts[vi]);
						if(side>0) add_unique(upper, vi);
						else add_unique(lower, vi);
					}
					goto done;
				}

				//does the segment uv cross edge ab?
				if(a==u||b==u) {
					//edge incident to u; the non-u vertex goes to a polygon
					int other=(a==u)?b:a;
					if(other==v) goto done;
					float side=cross(pu, pv, verts[other]);
					if(side>0) add_unique(upper, other);
					else add_unique(lower, other);
					continue;
				}

				if(segments_intersect(pu, pv, verts[a], verts[b])) {
					//the segment exits through edge ab
					//a and b go to opposite polygon chains
					add_unique(cross(pu, pv, verts[a])>0?upper:lower, a);
					add_unique(cross(pu, pv, verts[b])>0?upper:lower, b);

					//find adjacent tri across this edge
					next_tri=find_adjacent(cur_tri, a, b);
					peea=a, peeb=b;
					break;
				}
			}
			cur_tri=next_tri;
		}
	done:;

		//sort upper/lower by projection along uv
		auto proj_key=[&] (int idx) {
			float dx=pv.x-pu.x, dy=pv.y-pu.y;
			return dx*(verts[idx].x-pu.x)+dy*(verts[idx].y-pu.y);
		};
		std::sort(upper.begin(), upper.end(), [&] (int a, int b) {
			return proj_key(a)<proj_key(b);
		});
		std::sort(lower.begin(), lower.end(), [&] (int a, int b) {
			return proj_key(a)<proj_key(b);
		});
	}

	//find index of first tri incident to vertex u
	//that is "entered" when walking toward v
	int find_start_triangle(int u, int v) {
		const auto& pu=verts[u], & pv=verts[v];
		for(int i=0; i<tris.size(); i++) {
			const Tri& t=tris[i];
			bool has_u=(t[0]==u||t[1]==u||t[2]==u);
			if(!has_u) continue;

			//check if the interior of t is crossed by ray uv
			for(int e=0; e<3; e++) {
				int a=t[e], b=t[(e+1)%3];
				if(a==u||b==u) continue;

				if(segments_intersect(pu, pv, verts[a], verts[b])) return i;
			}
			//or t contains v directly
			if(t[0]==v||t[1]==v||t[2]==v) return i;
		}
		return -1;
	}

	//find adjacent tri across edge ab
	//i.e. the other tri with edge ab
	int find_adjacent(int tri_idx, int a, int b) {
		for(int i=0; i<tris.size(); i++) {
			if(i==tri_idx) continue;

			const Tri& t=tris[i];
			bool has_a=(t[0]==a||t[1]==a||t[2]==a);
			bool has_b=(t[0]==b||t[1]==b||t[2]==b);
			if(has_a&&has_b) return i;
		}
		return -1;
	}

	void retriangulate_polygon(int base_u, int base_v, const std::vector<int>& mid_verts) {
		//degenerate: no polygon on this side
		if(mid_verts.empty()) return;

		//build the ordered polygon
		std::vector<int> poly;
		poly.push_back(base_u);
		for(int v:mid_verts) poly.push_back(v);
		poly.push_back(base_v);

		retri_recursive(poly, 0, poly.size()-1);
	}

	void retri_recursive(const std::vector<int>& poly, int left, int right) {
		if(right-left<2) return;

		const auto& a=verts[poly[left]], & b=verts[poly[right]];

		int best=left+1;
		for(int k=left+2; k<right; k++) {
			if(in_circumcircle(verts[poly[k]], a, b, verts[poly[best]])) best=k;
		}

		tris.push_back(make_ccw(poly[left], poly[right], poly[best]));

		retri_recursive(poly, left, best);
		retri_recursive(poly, best, right);
	}
};

std::vector<Tri> constrainedDelaunayTriangulate(
	const std::vector<cmn::vf2d>& verts,
	const std::vector<Edge>& constraints) {
	CDT cdt;
	cdt.verts=verts;
	cdt.constraints=constraints;
	cdt.triangulate();
	return cdt.tris;
}
#endif