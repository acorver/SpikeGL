#ifndef Vec_H
#define Vec_H
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
};
#endif
