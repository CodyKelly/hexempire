//
// Created by hoy on 11/21/25.
//

#ifndef ATLAS_MATH_H
#define ATLAS_MATH_H

#include <SDL3/SDL_stdinc.h>

struct Vector2
{
    float x = 0, y = 0;

    [[nodiscard]] static float DistanceBetween(const Vector2& v1, const Vector2& v2)
    {
        const float dx = v2.x - v1.x;
        const float dy = v2.y - v1.y;
        return SDL_sqrtf(dx * dx + dy * dy);
    }

    [[nodiscard]] static float Dot(const Vector2& v1, const Vector2& v2)
    {
        return v1.x * v2.x + v1.y * v2.y;
    }

    Vector2() = default;

    Vector2(const float x, const float y) : x(x), y(y)
    {
    }

    [[nodiscard]] float MagnitudeSquared() const
    {
        return x * x + y * y;
    }

    [[nodiscard]] float Magnitude() const
    {
        return SDL_sqrtf(MagnitudeSquared());
    }

    [[nodiscard]] float DistanceBetween(const Vector2& v2) const { return DistanceBetween(*this, v2); }

    [[nodiscard]] Vector2 Scale(const Vector2& v2) const { return {x * v2.x, y * v2.y}; }

    [[nodiscard]] Vector2 operator*(const float f) const { return {x * f, y * f}; }
    [[nodiscard]] Vector2 operator/(const float f) const { return {x / f, y / f}; }
    [[nodiscard]] Vector2 operator+(const Vector2& v) const { return {x + v.x, y + v.y}; }
    [[nodiscard]] Vector2 operator-(const Vector2& v) const { return {x - v.x, y - v.y}; }
    [[nodiscard]] Vector2 operator-() const { return {-x, -y}; }

    Vector2& operator+=(const Vector2& v)
    {
        x += v.x;
        y += v.y;
        return *this;
    }

    Vector2& operator-=(const Vector2& v)
    {
        x -= v.x;
        y -= v.y;
        return *this;
    }

    Vector2& operator*=(const float f)
    {
        x *= f;
        y *= f;
        return *this;
    }

    Vector2& operator/=(const float f)
    {
        x /= f;
        y /= f;
        return *this;
    }

    [[nodiscard]] bool operator==(const Vector2& v) const { return x == v.x && y == v.y; }
    [[nodiscard]] bool operator!=(const Vector2& v) const { return !(*this == v); }

    // Dot product
    [[nodiscard]] float Dot(const Vector2& v) const { return x * v.x + y * v.y; }

    // Normalize
    [[nodiscard]] Vector2 Normalized() const
    {
        const float len = Magnitude();
        return len > 0 ? *this / len : Vector2(0, 0);
    }

    // Distance squared (faster when you don't need actual distance)
    [[nodiscard]] float DistanceSquared(const Vector2& v2) const
    {
        const float dx = v2.x - x;
        const float dy = v2.y - y;
        return dx * dx + dy * dy;
    }

    [[nodiscard]] static Vector2 Lerp(const Vector2& a, const Vector2& b, const float t)
    {
        return a + (b - a) * t;
    }
};

struct Matrix4x4
{
    float m11, m12, m13, m14;
    float m21, m22, m23, m24;
    float m31, m32, m33, m34;
    float m41, m42, m43, m44;
};

inline Matrix4x4 Matrix4x4_CreateOrthographicOffCenter(const Vector2& size, const Vector2& scale)
{
    return (Matrix4x4){
        2.0f / size.x, 0, 0, 0,
        0, -2.0f / size.y, 0, 0,
        0, 0, 1, 0,
        -1, 1, 0, 1
    };
}


#endif //ATLAS_MATH_H
