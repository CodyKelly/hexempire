#include "CameraSystem.h"

Camera::Camera(Vector2 viewportSize)
{
    _state.viewportSize = viewportSize;
    _state.current.scale = {1.0f, 1.0f};
    _state.target.scale = {1.0f, 1.0f};
}

const Matrix4x4& Camera::GetViewProjectionMatrix()
{
    if (_matricesDirty)
    {
        _matrices.viewProjection = Matrix4x4_CreateOrthographicOffCenter(
            _state.viewportSize,
            _state.current.scale,
            _state.current.position
        );
        _matricesDirty = false;
    }
    return _matrices.viewProjection;
}

void Camera::SetViewportSize(Vector2 size)
{
    if (_state.viewportSize != size)
    {
        _state.viewportSize = size;
        _matricesDirty = true;
    }
}

void Camera::SetPosition(Vector2 position)
{
    _state.current.position = position;
    _state.target.position = position;
    _matricesDirty = true;
}

void Camera::SetTargetPosition(Vector2 position)
{
    _state.target.position = position;
}

void Camera::SetScale(Vector2 scale)
{
    _state.current.scale = scale;
    _state.target.scale = scale;
    _matricesDirty = true;
}

void Camera::SetTargetScale(Vector2 scale)
{
    _state.target.scale = scale;
}

Vector2 Camera::ScreenToWorld(Vector2 screenPos) const
{
    // Convert screen position to world position
    // Screen origin is top-left, world origin depends on camera position
    Vector2 normalized = {
        screenPos.x / _state.viewportSize.x,
        screenPos.y / _state.viewportSize.y
    };

    Vector2 worldSize = _state.viewportSize.Scale(_state.current.scale);
    return _state.current.position + normalized.Scale(worldSize);
}

Vector2 Camera::WorldToScreen(Vector2 worldPos) const
{
    Vector2 worldSize = _state.viewportSize.Scale(_state.current.scale);
    Vector2 relative = worldPos - _state.current.position;
    return {
        (relative.x / worldSize.x) * _state.viewportSize.x,
        (relative.y / worldSize.y) * _state.viewportSize.y
    };
}

// CameraController implementation

CameraController::CameraController(Camera* camera, CameraConfig config)
    : _camera(camera), _config(config)
{
}

void CameraController::Pan(Vector2 delta)
{
    if (!_camera) return;

    // Convert screen delta to world delta based on current scale
    Vector2 worldDelta = delta.Scale(_camera->GetScale());
    _camera->SetTargetPosition(_camera->GetState().target.position + worldDelta);
}

void CameraController::Zoom(float delta)
{
    if (!_camera) return;

    Vector2 currentScale = _camera->GetState().target.scale;
    float newZoom = currentScale.x * (1.0f - delta * _config.zoomSpeed);
    newZoom = SDL_clamp(newZoom, _config.zoomMin, _config.zoomMax);

    _camera->SetTargetScale({newZoom, newZoom});
}

void CameraController::ZoomToPoint(float delta, Vector2 screenPoint)
{
    if (!_camera) return;

    // Get world position under cursor before zoom
    Vector2 worldBefore = _camera->ScreenToWorld(screenPoint);

    // Apply zoom
    Zoom(delta);

    // After zoom, calculate where that world point would be on screen
    // Adjust camera position so the world point stays under cursor
    // This requires updating current scale first for the calculation
    Vector2 targetScale = _camera->GetState().target.scale;
    Vector2 normalized = {
        screenPoint.x / _camera->GetViewportSize().x,
        screenPoint.y / _camera->GetViewportSize().y
    };
    Vector2 worldSize = _camera->GetViewportSize().Scale(targetScale);
    Vector2 newPosition = worldBefore - normalized.Scale(worldSize);

    _camera->SetTargetPosition(newPosition);
}

void CameraController::Update(float deltaTime)
{
    if (!_camera) return;

    CameraState& state = _camera->GetState();

    // Smoothly interpolate toward target
    float t = 1.0f - SDL_expf(-_config.smoothing * deltaTime);

    bool changed = false;

    if (state.current.position != state.target.position)
    {
        state.current.position = Vector2::Lerp(state.current.position, state.target.position, t);
        changed = true;
    }

    if (state.current.scale != state.target.scale)
    {
        state.current.scale = Vector2::Lerp(state.current.scale, state.target.scale, t);
        changed = true;
    }

    if (changed)
    {
        _camera->MarkDirty();
    }
}

void CameraController::SnapToTarget()
{
    if (!_camera) return;

    CameraState& state = _camera->GetState();
    state.current = state.target;
    _camera->MarkDirty();
}

void CameraController::Follow(Vector2 worldPosition, float deltaTime)
{
    if (!_camera) return;

    // Center the camera on the target position
    Vector2 worldSize = _camera->GetViewportSize().Scale(_camera->GetScale());
    Vector2 targetPos = worldPosition - worldSize * 0.5f;

    _camera->SetTargetPosition(targetPos);
    Update(deltaTime);
}
