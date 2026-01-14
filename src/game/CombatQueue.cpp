//
// CombatQueue.cpp - Combat action queue implementation
//

#include "CombatQueue.h"

void CombatQueue::QueueAction(const CombatAction& action) {
    _pendingActions.push(action);

    // If this is the first action, start the timer
    if (_pendingActions.size() == 1) {
        _timer = _processingDelay;
        // If delay is 0, action is ready immediately
        _actionReady = (_processingDelay <= 0.0f);
    }
}

bool CombatQueue::HasPendingActions() const {
    return !_pendingActions.empty();
}

bool CombatQueue::IsProcessing() const {
    return !_pendingActions.empty();
}

void CombatQueue::Update(float deltaTime) {
    if (_pendingActions.empty()) {
        _actionReady = false;
        return;
    }

    _timer -= deltaTime;
    if (_timer <= 0) {
        _actionReady = true;
    }
}

std::optional<CombatAction> CombatQueue::PopNextAction() {
    if (!_actionReady || _pendingActions.empty()) {
        return std::nullopt;
    }

    CombatAction action = _pendingActions.front();
    _pendingActions.pop();

    // Reset timer for next action if queue still has items
    if (!_pendingActions.empty()) {
        _timer = _processingDelay;
        // If delay is 0, next action is ready immediately
        _actionReady = (_processingDelay <= 0.0f);
    } else {
        _actionReady = false;
    }

    return action;
}

void CombatQueue::SetProcessingDelay(float delay) {
    _processingDelay = delay;
}

float CombatQueue::GetProcessingDelay() const {
    return _processingDelay;
}

void CombatQueue::Clear() {
    while (!_pendingActions.empty()) {
        _pendingActions.pop();
    }
    _timer = 0.0f;
    _actionReady = false;
}

size_t CombatQueue::GetQueueSize() const {
    return _pendingActions.size();
}
