#ifndef Vec_H
#define Vec_H
#include "Util.h"
struct Vec2 
{
    union {
        struct {
            double v1, v2;
        };
        struct {
            double x, y;
        };
    };
    Vec2() : x(0.), y(0.) {}
    Vec2(double x, double y) : x(x), y(y) {}
	bool operator==(const Vec2 & o) const { return feq(v1,o.v1) && feq(v2,o.v2); }
	bool operator!=(const Vec2 & o) const { return !((*this) == o); }
};
struct Vec2f
{
    union {
        struct {
            float v1, v2;
        };
        struct {
            float x, y;
        };
    };
    Vec2f() : x(0.), y(0.) {}
    Vec2f(float x, float y) : x(x), y(y) {}
	bool operator==(const Vec2f & o) const { return feqf(v1,o.v1) && feqf(v2,o.v2); }
	bool operator!=(const Vec2f & o) const { return !((*this) == o); }
};

struct Vec3 
{
    union {
        struct {
            double v1, v2, v3;
        };
        struct {
            double x, y, z;
        };
    };
    Vec3() : x(0.), y(0.), z(0.) {}
    Vec3(double x, double y, double z) : x(x), y(y), z(z) {}
	bool operator==(const Vec3 & o) const { return feq(v1,o.v1) && feq(v2,o.v2) && feq(v3,o.v3); }
	bool operator!=(const Vec3 & o) const { return !((*this) == o); }
};
struct Vec3f
{
    union {
        struct {
            float v1, v2, v3;
        };
        struct {
            float x, y, z;
        };
    };
    Vec3f() : x(0.f), y(0.f), z(0.f) {}
    Vec3f(float x, float y, float z) : x(x), y(y), z(z) {}
	bool operator==(const Vec3f & o) const { return feqf(v1,o.v1) && feqf(v2,o.v2) && feqf(v3,o.v3); }
	bool operator!=(const Vec3f & o) const { return !((*this) == o); }
};

struct Vec4 
{
    union {
        struct {
            double v1, v2, v3, v4;
        };
        struct {
            double x, y, z, w;
        };
    };
    Vec4() : x(0.), y(0.), z(0.), w(0.) {}
    Vec4(double x, double y, double z, double w) : x(x), y(y), z(z), w(w) {}
	bool operator==(const Vec4 & o) const { return feq(v1,o.v1) && feq(v2,o.v2) && feq(v3,o.v3) && feq(v4,o.v4); }
	bool operator!=(const Vec4 & o) const { return !((*this) == o); }
};
struct Vec4f
{
    union {
        struct {
            float v1, v2, v3, v4;
        };
        struct {
            float x, y, z, w;
        };
    };
    Vec4f() : x(0.f), y(0.f), z(0.f), w(0.) {}
    Vec4f(float x, float y, float z, float w) : x(x), y(y), z(z), w(w) {}
	bool operator==(const Vec4f & o) const { return feqf(v1,o.v1) && feqf(v2,o.v2) && feqf(v3,o.v3) && feqf(v4,o.v4); }
	bool operator!=(const Vec4f & o) const { return !((*this) == o); }
};
#endif
