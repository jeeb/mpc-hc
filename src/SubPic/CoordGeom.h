/*
 * (C) 2003-2006 Gabest
 * (C) 2006-2013 see Authors.txt
 *
 * This file is part of MPC-HC.
 *
 * MPC-HC is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * MPC-HC is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#pragma once

#include <cmath>

// basic conversions
// DegToRad() multiplies by Pi/180
// RadToDeg() multiplies by 180/Pi
// note: there are sufficient digits in these decimal representations to have full precion in the IEEE 754 quadruple-precision binary floating-point format

__forceinline float DegToRad(float in)
{
    return in * 0.0174532925199432957692369076848861271344287188854f;
}
__forceinline float RadToDeg(float in)
{
    return in * 57.295779513082320876798154814105170332405472466564f;
}

__forceinline double DegToRad(double in)
{
    return in * 0.0174532925199432957692369076848861271344287188854;
}
__forceinline double RadToDeg(double in)
{
    return in * 57.295779513082320876798154814105170332405472466564;
}

__forceinline long double DegToRad(long double in)
{
    return in * 0.0174532925199432957692369076848861271344287188854L;
}
__forceinline long double RadToDeg(long double in)
{
    return in * 57.295779513082320876798154814105170332405472466564L;
}

struct Vector {
    double x, y, z;

    Vector() {
        x = y = z = 0.0;
    }
    Vector(double x, double y, double z) {
        this->x = x;
        this->y = y;
        this->z = z;
    }
    void Set(double x, double y, double z) {
        this->x = x;
        this->y = y;
        this->z = z;
    }

    Vector Normal(const Vector& a, const Vector& b) const {
        return ((a - *this) % (b - a));
    }
    double Angle(const Vector& a, const Vector& b) const {
        return (((a - *this).Unit()).Angle((b - *this).Unit()));
    }
    double Angle(const Vector& a) const {
        double angle = *this | a;
        return (angle > 1.0) ? 0.0
               : (angle < -1.0) ? 3.1415926535897932384626433832795028841971693993751
               : acos(angle);
    }
    void Angle(double& u, double& v) const {// returns spherical coords in radians, -PI/2 <= u <= PI/2, -PI <= v <= PI
        Vector n = Unit();

        u = asin(n.y);
        if (!n.z) {
            v = _copysign(1.5707963267948966192313216916397514420985846996876, n.x);// PI/2
        } else if (n.z > 0) {
            v = atan(n.x / n.z);
        } else {
            v = _copysign(3.1415926535897932384626433832795028841971693993751, n.x) + atan(n.x / n.z);
        }
    }
    Vector Angle() const {// does like previous, returns 'u' in 'ret.x', and 'v' in 'ret.y'
        Vector ret;
        Angle(ret.x, ret.y);
        ret.z = 0.0;
        return ret;
    }

    Vector Unit() const {
        double l = Length();
        if (!l || l == 1.0) {
            return *this;
        }
        return (*this * (1 / l));
    }
    double Length() const {
        return sqrt(x * x + y * y + z * z);
    }
    double Sum() const {
        return (x + y + z);
    }
    double CrossSum() const {
        return (x * y + x * z + y * z);
    }
    Vector Cross() const {
        return Vector(x * y, x * z, y * z);
    }
    Vector Pow(double exp) const {
        return (!exp) ? Vector(1.0, 1.0, 1.0) : (exp == 1.0) ? *this : Vector(pow(x, exp), pow(y, exp), pow(z, exp));
    }

    Vector Min(const Vector& a) const {
        return Vector((x < a.x) ? x : a.x,
                      (y < a.y) ? y : a.y,
                      (z < a.z) ? z : a.z);
    }
    Vector Max(const Vector& a) const {
        return Vector((x > a.x) ? x : a.x,
                      (y > a.y) ? y : a.y,
                      (z > a.z) ? z : a.z);
    }
    Vector Abs() const {
        return Vector(fabs(x), fabs(y), fabs(z));
    }

    Vector Reflect(const Vector& N) const {
        return (N * ((-*this) | N) * 2.0 - (-*this));
    }
    Vector Refract(const Vector& N, double nFront, double nBack, double* nOut) const {
        const Vector D = -*this;

        double N_dot_D = (N | D);
        double n = N_dot_D >= 0 ? (nFront / nBack) : (nBack / nFront);

        Vector cos_D = N * N_dot_D;
        Vector sin_T = (cos_D - D) * n;

        double len_sin_T = sin_T | sin_T;

        if (len_sin_T > 1.0) {
            if (nOut) {
                *nOut = (N_dot_D >= 0.0) ? nFront : nBack;
            }
            return (*this).Reflect(N);
        }

        double N_dot_T = sqrt(1.0 - len_sin_T);
        if (N_dot_D < 0) { N_dot_T = -N_dot_T; }

        if (nOut) {
            *nOut = (N_dot_D >= 0) ? nBack : nFront;
        }

        return (sin_T - (N * N_dot_T));
    }
    Vector Refract2(const Vector& N, double nFrom, double nTo, double* nOut) const {
        Vector D = -*this;

        double N_dot_D = N | D;
        double n = nFrom / nTo;

        Vector cos_D = N * N_dot_D;
        Vector sin_T = (cos_D - D) * n;

        double len_sin_T = sin_T | sin_T;

        if (len_sin_T > 1.0) {
            if (nOut) {
                *nOut = nFrom;
            }
            return (*this).Reflect(N);
        }

        double N_dot_T = sqrt(1.0 - len_sin_T);

        if (nOut) {
            *nOut = nTo;
        }

        return (sin_T - (N * N_dot_T));
    }

    Vector operator - () const {
        return Vector(-x, -y, -z);
    }

    double operator | (const Vector& v) const {// dot
        return (x * v.x + y * v.y + z * v.z);
    }
    Vector operator % (const Vector& v) const {// cross
        return Vector(y * v.z - z * v.y, z * v.x - x * v.z, x * v.y - y * v.x);
    }

    bool operator == (const Vector& v) const {
        if (x - v.x || y - v.y || z - v.z) {
            return false;
        }
        return true;
    }
    bool operator != (const Vector& v) const {
        return (*this == v) ? false : true;
    }

    Vector operator + (double d) const {
        return Vector(x + d, y + d, z + d);
    }
    Vector operator + (const Vector& v) const {
        return Vector(x + v.x, y + v.y, z + v.z);
    }
    Vector operator - (double d) const {
        return Vector(x - d, y - d, z - d);
    }
    Vector operator - (const Vector& v) const {
        return Vector(x - v.x, y - v.y, z - v.z);
    }
    Vector operator * (double d) const {
        return Vector(x * d, y * d, z * d);
    }
    Vector operator * (const Vector& v) const {
        return Vector(x * v.x, y * v.y, z * v.z);
    }
    Vector operator / (double d) const {
        return Vector(x / d, y / d, z / d);
    }
    Vector operator / (const Vector& v) const {
        return Vector(x / v.x, y / v.y, z / v.z);
    }
    Vector& operator += (double d) {
        x += d;
        y += d;
        z += d;
        return *this;
    }
    Vector& operator += (const Vector& v) {
        x += v.x;
        y += v.y;
        z += v.z;
        return *this;
    }
    Vector& operator -= (double d) {
        x -= d;
        y -= d;
        z -= d;
        return *this;
    }
    Vector& operator -= (const Vector& v) {
        x -= v.x;
        y -= v.y;
        z -= v.z;
        return *this;
    }
    Vector& operator *= (double d) {
        x *= d;
        y *= d;
        z *= d;
        return *this;
    }
    Vector& operator *= (const Vector& v) {
        x *= v.x;
        y *= v.y;
        z *= v.z;
        return *this;
    }
    Vector& operator /= (double d) {
        x /= d;
        y /= d;
        z /= d;
        return *this;
    }
    Vector& operator /= (const Vector& v) {
        x /= v.x;
        y /= v.y;
        z /= v.z;
        return *this;
    }
};

struct Ray {
    Vector p, d;

    Ray() {}
    Ray(const Vector& p, const Vector& d) {
        this->p = p;
        this->d = d;
    }
    void Set(const Vector& p, const Vector& d) {
        this->p = p;
        this->d = d;
    }

    double GetDistanceFrom(const Ray& r) const {// r = plane
        double t = d | r.d;
        return ((r.p - p) | r.d) / t;
    }
    double GetDistanceFrom(const Vector& v) const {// v = point
        double t = ((v - p) | d) / (d | d);
        return ((p + d * t) - v).Length();
    }

    Vector operator [](double t) const {
        return p + d * t;
    }
};

struct XForm {
    struct Matrix {
        double mat[4][4];

        Matrix() { Initalize(); }
        void Initalize() {
            mat[0][0] = mat[1][1] = mat[2][2] = mat[3][3] = 1.0;
            mat[0][1] = mat[0][2] = mat[0][3] = mat[1][0] = mat[1][2] = mat[1][3] = mat[2][0] = mat[2][1] = mat[2][3] = mat[3][0] = mat[3][1] = mat[3][2] = 0.0;
        }

        Matrix operator * (const Matrix& m) const {
            Matrix ret;
            for (ptrdiff_t i = 0; i < 4; i++) {
                for (ptrdiff_t j = 0; j < 4; j++) {
                    ret.mat[i][j] = mat[i][0] * m.mat[0][j] + mat[i][1] * m.mat[1][j] + mat[i][2] * m.mat[2][j] + mat[i][3] * m.mat[3][j];
                    if (!ret.mat[i][j]) {
                        ret.mat[i][j] = 0;
                    }
                }
            }
            return ret;
        }
        Matrix& operator *= (const Matrix& m) {
            return (*this = *this * m);
        }
    } m;
    bool m_isWorldToLocal;

    XForm() : m_isWorldToLocal(true) {}
    XForm(const Ray& r, const Vector& s, bool isWorldToLocal) {
        Initalize(r, s, isWorldToLocal);
    }

    void Initalize() { m.Initalize(); }
    void Initalize(const Ray& r, const Vector& s, bool isWorldToLocal) {
        m.Initalize();

        m_isWorldToLocal = isWorldToLocal;
        if (isWorldToLocal) {
            *this -= r.p;
            *this >>= r.d;
            *this /= s;
        } else {
            *this *= s;
            *this <<= r.d;
            *this += r.p;
        }
    }

    bool operator == (const XForm& f) const {
        ptrdiff_t i = 3;
        do {
            ptrdiff_t j = 3;
            do {
                if (m.mat[i][j] != f.m.mat[i][j]) {
                    return false;
                }
            } while (--j >= 0);
        } while (--i >= 0);
        return true;
    }
    bool operator != (const XForm& f) const {
        return (*this == f) ? false : true;
    }

    void operator *= (const Vector& v) {// scale
        Matrix s;
        s.mat[0][0] = v.x;
        s.mat[1][1] = v.y;
        s.mat[2][2] = v.z;
        m *= s;
    }
    void operator += (const Vector& v) {// translate
        Matrix t;
        t.mat[3][0] = v.x;
        t.mat[3][1] = v.y;
        t.mat[3][2] = v.z;
        m *= t;
    }
    void operator <<= (const Vector& v) {// rotate
        Matrix x;
        x.mat[1][1] = cos(v.x);
        x.mat[1][2] = -sin(v.x);
        x.mat[2][1] = sin(v.x);
        x.mat[2][2] = cos(v.x);

        Matrix y;
        y.mat[0][0] = cos(v.y);
        y.mat[0][2] = -sin(v.y);
        y.mat[2][0] = sin(v.y);
        y.mat[2][2] = cos(v.y);

        Matrix z;
        z.mat[0][0] = cos(v.z);
        z.mat[0][1] = -sin(v.z);
        z.mat[1][0] = sin(v.z);
        z.mat[1][1] = cos(v.z);

        m = m_isWorldToLocal ? (m * y * x * z) : (m * z * x * y);
    }

    void operator /= (const Vector& v) {// scale
        Vector s;
        s.x = (!v.x) ? 0.0 : 1.0 / v.x;
        s.y = (!v.y) ? 0.0 : 1.0 / v.y;
        s.z = (!v.z) ? 0.0 : 1.0 / v.z;
        *this *= s;
    }
    void operator -= (const Vector& v) {// translate
        Matrix t;
        t.mat[3][0] = -v.x;
        t.mat[3][1] = -v.y;
        t.mat[3][2] = -v.z;
        m *= t;
    }
    void operator >>= (const Vector& v) {// rotate
        *this <<= -v;
    }

    //  transformations
    Vector operator < (const Vector& v) const {// normal
        Vector ret;
        ret.x = v.x * m.mat[0][0] + v.y * m.mat[1][0] + v.z * m.mat[2][0];
        ret.y = v.x * m.mat[0][1] + v.y * m.mat[1][1] + v.z * m.mat[2][1];
        ret.z = v.x * m.mat[0][2] + v.y * m.mat[1][2] + v.z * m.mat[2][2];
        return ret;
    }
    Vector operator << (const Vector& v) const {// vector
        Vector ret;
        ret.x = v.x * m.mat[0][0] + v.y * m.mat[1][0] + v.z * m.mat[2][0] + m.mat[3][0];
        ret.y = v.x * m.mat[0][1] + v.y * m.mat[1][1] + v.z * m.mat[2][1] + m.mat[3][1];
        ret.z = v.x * m.mat[0][2] + v.y * m.mat[1][2] + v.z * m.mat[2][2] + m.mat[3][2];
        return ret;
    }
    Ray operator << (const Ray& r) const {// ray
        return Ray(*this << r.p, *this < r.d);
    }
};
