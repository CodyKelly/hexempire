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
        _viewMatrix = Matrix4x4_CreateOrthographicOffCenter(_size, _transform.scale, {0, 0});
        _viewMatrixDirty = false;
    }
    return _viewMatrix;
}

void Camera::SetSize(const Vector2& size)
{
    _size = size;
}

void Camera::SetScale(const Vector2& scale)
{
    _transform.scale = scale;
    _viewMatrixDirty = true;
}

