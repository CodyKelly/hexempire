//
// Created by hoy on 11/23/25.
//

#include <SDL3/SDL_rect.h>

#include "Camera.h"
#include "math.h"

Matrix4x4 Camera::GetViewMatrix()
{
    if (_viewMatrixDirty)
    {
        _viewMatrix = Matrix4x4_CreateOrthographicOffCenter(_size, _transform.scale, _transform.position);
        _viewMatrixDirty = false;
    }
    return _viewMatrix;
}

void Camera::SetSize(const Vector2& size)
{
    _size = size;
    _viewMatrixDirty = true;
}

void Camera::SetScale(const Vector2& scale)
{
    _transform.scale = scale;
    _viewMatrixDirty = true;
}

void Camera::SetPosition(const Vector2& position)
{
    _transform.position = position;
    _viewMatrixDirty = true;
}

void Camera::Move(const Vector2& delta)
{
    _transform.position += delta;
    _viewMatrixDirty = true;
}
