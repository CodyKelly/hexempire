//
// CombatQueue.h - Combat action queue for sequential processing
//

#ifndef ATLAS_COMBATQUEUE_H
#define ATLAS_COMBATQUEUE_H

#include "GameData.h"
#include <queue>
#include <optional>

class CombatQueue {
public:
    CombatQueue() = default;

    // Queue a new combat action
    void QueueAction(const CombatAction& action);

    // Check if there are pending actions
    [[nodiscard]] bool HasPendingActions() const;

    // Check if the queue is actively processing (has items)
    [[nodiscard]] bool IsProcessing() const;

    // Update the queue timer
    void Update(float deltaTime);

    // Get the next action if ready (timer expired and queue non-empty)
    [[nodiscard]] std::optional<CombatAction> PopNextAction();

    // Configuration
    void SetProcessingDelay(float delay);
    [[nodiscard]] float GetProcessingDelay() const;

    // Queue management
    void Clear();
    [[nodiscard]] size_t GetQueueSize() const;

private:
    std::queue<CombatAction> _pendingActions;
    float _processingDelay = 0.0f;  // Delay between processing each action (0 = instant)
    float _timer = 0.0f;
    bool _actionReady = false;
};

#endif // ATLAS_COMBATQUEUE_H
