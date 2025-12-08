//
// Created by hoy on 11/23/25.
//

#ifndef ATLAS_CAMERA_H
#define ATLAS_CAMERA_H

#include <SDL3/SDL_rect.h>

#include "math.h"
#include "Transform.h"

class Camera
{
private:
    Transform _transform;
    Vector2 _size;
    Matrix4x4 _viewMatrix;
    bool _viewMatrixDirty = true;

public:
    Camera() = default;

    Camera(const Vector2 size) : _size(size), _viewMatrix()
    {
    };

    Matrix4x4 GetViewMatrix();

    void SetSize(const Vector2& size);
    void SetScale(const Vector2& scale);
    Vector2 GetScale() const { return _transform.scale; }

    void SetPosition(const Vector2& position);
    Vector2 GetPosition() const { return _transform.position; }
    void Move(const Vector2& delta);
};


#endif //ATLAS_CAMERA_H
