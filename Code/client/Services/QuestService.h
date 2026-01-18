#pragma once

#include <Events/EventDispatcher.h>
#include <Games/Events.h>
#include <World.h>

struct NotifyQuestUpdate;
struct NotifyQuestSceneUpdate;

struct TESQuest;

/**
 * @brief Handles quest sync
 */
class QuestService final : public BSTEventSink<TESQuestStartStopEvent>, BSTEventSink<TESQuestStageEvent>, BSTEventSink<TESSceneEvent>, BSTEventSink<TESSceneActionEvent>, BSTEventSink<TESScenePhaseEvent>
{
public:
    QuestService(World&, entt::dispatcher&);
    ~QuestService() = default;

    static bool IsNonSyncableQuest(TESQuest* apQuest);
    static void DebugDumpQuests();
    static bool StopQuest(uint32_t aformId);

private:
    friend struct QuestEventHandler;

    void OnConnected(const ConnectedEvent&) noexcept;

    BSTEventResult OnEvent(const TESQuestStartStopEvent*, const EventDispatcher<TESQuestStartStopEvent>*) override;
    BSTEventResult OnEvent(const TESQuestStageEvent*, const EventDispatcher<TESQuestStageEvent>*) override;
    BSTEventResult OnEvent(const TESSceneEvent*, const EventDispatcher<TESSceneEvent>*) override;
    BSTEventResult OnEvent(const TESSceneActionEvent*, const EventDispatcher<TESSceneActionEvent>*) override;
    BSTEventResult OnEvent(const TESScenePhaseEvent*, const EventDispatcher<TESScenePhaseEvent>*) override;

    void OnQuestUpdate(const NotifyQuestUpdate&) noexcept;
    void OnQuestSceneUpdate(const NotifyQuestSceneUpdate&) noexcept;

    bool CanAdvanceQuestForParty() const noexcept;

    World& m_world;

    entt::scoped_connection m_joinedConnection;
    entt::scoped_connection m_leftConnection;
    entt::scoped_connection m_questUpdateConnection;
    entt::scoped_connection m_questSceneUpdateConnection;
};
