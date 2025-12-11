#ifndef ATLAS_CAMERASYSTEM_H
#define ATLAS_CAMERASYSTEM_H

#include "math.h"
#include "Transform.h"

// Pure data: Camera configuration
struct CameraConfig
{
    float zoomMin = 0.1f;
    float zoomMax = 10.0f;
    float zoomSpeed = 0.1f;
    float moveSpeed = 500.0f;
    float smoothing = 5.0f; // For lerp-based smoothing
};

// Pure data: Camera runtime state
struct CameraState
{
    Transform current; // Current transform (rendered)
    Transform target; // Target transform (where we want to be)
    Vector2 velocity; // Current velocity for momentum-based movement
    Vector2 viewportSize; // Screen dimensions
    bool isDirty = true;
};

// Pure data: Cached matrices
struct CameraMatrices
{
    Matrix4x4 view;
    Matrix4x4 projection;
    Matrix4x4 viewProjection;
};

// The Camera class now primarily holds data and computes matrices
// Controller logic is separate
class Camera
{
private:
    CameraState _state;
    CameraMatrices _matrices;
    bool _matricesDirty = true;

public:
    Camera() = default;
    explicit Camera(Vector2 viewportSize);

    // Getters for state
    const CameraState& GetState() const { return _state; }
    CameraState& GetState() { return _state; }

    // Matrix access (lazy evaluation)
    const Matrix4x4& GetViewProjectionMatrix();

    // Direct setters that mark dirty
    void SetViewportSize(Vector2 size);
    void SetPosition(Vector2 position);
    void SetTargetPosition(Vector2 position);
    void SetScale(Vector2 scale);
    void SetTargetScale(Vector2 scale);

    // Convenience
    Vector2 GetPosition() const { return _state.current.position; }
    Vector2 GetScale() const { return _state.current.scale; }
    Vector2 GetViewportSize() const { return _state.viewportSize; }

    // Convert screen coords to world coords
    Vector2 ScreenToWorld(Vector2 screenPos) const;
    Vector2 WorldToScreen(Vector2 worldPos) const;

    void MarkDirty() { _matricesDirty = true; }
};

// System: Updates camera based on input and config
// This is the "behavior" separated from data
class CameraController
{
private:
    Camera* _camera;
    CameraConfig _config;

public:
    CameraController() : _camera(nullptr)
    {
    }

    CameraController(Camera* camera, CameraConfig config = {});

    void SetCamera(Camera* camera) { _camera = camera; }
    void SetConfig(const CameraConfig& config) { _config = config; }
    const CameraConfig& GetConfig() const { return _config; }

    // Input handlers
    void Pan(Vector2 delta); // Move by screen-space delta
    void Zoom(float delta); // Zoom in/out
    void ZoomToPoint(float delta, Vector2 screenPoint); // Zoom toward cursor

    // Update: smoothly interpolate current toward target
    void Update(float deltaTime);

    // Snap current to target (no smoothing)
    void SnapToTarget();

    // Follow a world position
    void Follow(Vector2 worldPosition, float deltaTime);
};

#endif //ATLAS_CAMERASYSTEM_H
