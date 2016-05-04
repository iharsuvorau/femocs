/*
 * Vec3.h
 *
 *  Created on: 29.04.2016
 *  Author: scratchapixel
 *  http://www.scratchapixel.com/code.php?id=9&origin=/lessons/3d-basic-rendering/ray-tracing-rendering-a-triangle&src=1
 */

#ifndef VEC3_H_
#define VEC3_H_

template<typename T>
class Vec3 {
public:
    // Constructors
    Vec3() : x(T(0)), y(T(0)), z(T(0)) {}
    Vec3(T xx) : x(xx), y(xx), z(xx) {}

    Vec3(T xx, T yy, T zz) : x(xx), y(yy), z(zz) {}

    // Definition of + operator
    Vec3 operator + (const Vec3 &v) const
    { return Vec3(x + v.x, y + v.y, z + v.z); }

    // Definition of - operator
    Vec3 operator - (const Vec3 &v) const
    { return Vec3(x - v.x, y - v.y, z - v.z); }
    Vec3 operator - () const
    { return Vec3(-x, -y, -z); }

    // Definition of * operator
    Vec3 operator * (const T &r) const
    { return Vec3(x * r, y * r, z * r); }
    Vec3 operator * (const Vec3 &v) const
    { return Vec3(x * v.x, y * v.y, z * v.z); }
    Vec3& operator *= (const T &r)
    { x *= r, y *= r, z *= r; return *this; }
    friend Vec3 operator * (const T &r, const Vec3 &v)
    { return Vec3<T>(v.x * r, v.y * r, v.z * r); }

    // Definition of / operator
    Vec3 operator / (const T &r) const
    { return Vec3(x / r, y / r, z / r); }
    Vec3& operator /= (const T &r)
    { x /= r, y /= r, z /= r; return *this; }
    friend Vec3 operator / (const T &r, const Vec3 &v)
    { return Vec3<T>(r / v.x, r / v.y, r / v.z); }

    // Dot product, cross product, norm and length
    T dotProduct(const Vec3<T> &v) const
    { return x * v.x + y * v.y + z * v.z; }
    Vec3 crossProduct(const Vec3<T> &v) const
    { return Vec3<T>(y * v.z - z * v.y, z * v.x - x * v.z, x * v.y - y * v.x); }
    T norm() const
    { return x * x + y * y + z * z; }
    T length() const
    { return sqrt(norm()); }

    Vec3& normalize() {
        T n = norm();
        if (n > 0) {
            T factor = 1 / sqrt(n);
            x *= factor, y *= factor, z *= factor;
        }
        return *this;
    }

    /*
      The next two operators are called access operators or accessors.
      The Vec coordinates can be accessed that way v[0], v[1], v[2], rather than v.x, v.y, v.z.
      This useful when vectors are used in loops: the coordinates can be accessed with the loop index (e.g. v[i]).
    */
    const T& operator [] (uint8_t i) const { return (&x)[i]; }
    T& operator [] (uint8_t i) { return (&x)[i]; }

    // Define cout behaviour
    friend std::ostream& operator << (std::ostream &s, const Vec3<T> &v)
    { return s << '[' << v.x << ' ' << v.y << ' ' << v.z << ']'; }

    T x, y, z;
};

typedef Vec3<double> Vec3d;
typedef Vec3<float> Vec3f; 
typedef Vec3<int> Vec3i; 

#endif /* VEC3_H_ */
 
