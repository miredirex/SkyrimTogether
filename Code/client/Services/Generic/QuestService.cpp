#include <TiltedOnlinePCH.h>

#include <Events/ConnectedEvent.h>

#include <Services/QuestService.h>
#include <Services/ImguiService.h>

#include <PlayerCharacter.h>
#include <Forms/TESQuest.h>
#include <Games/TES.h>
#include <Games/Overrides.h>
#include <Games/Skyrim/AI/Movement/PlayerControls.h>

#include <Events/EventDispatcher.h>

#include <Messages/RequestQuestUpdate.h>
#include <Messages/NotifyQuestUpdate.h>

static TESQuest* FindQuestByNameId(const String& name)
{
    auto& questRegistry = ModManager::Get()->quests;
    auto it = std::find_if(questRegistry.begin(), questRegistry.end(), [name](auto* it) { return std::strcmp(it->idName.AsAscii(), name.c_str()); });

    return it != questRegistry.end() ? *it : nullptr;
}

QuestService::QuestService(World& aWorld, entt::dispatcher& aDispatcher)
    : m_world(aWorld)
{
    m_joinedConnection = aDispatcher.sink<ConnectedEvent>().connect<&QuestService::OnConnected>(this);
    m_questUpdateConnection = aDispatcher.sink<NotifyQuestUpdate>().connect<&QuestService::OnQuestUpdate>(this);

    // A note about the Gameevents:
    // TESQuestStageItemDoneEvent gets fired to late, we instead use TESQuestStageEvent, because it responds immediately.
    // TESQuestInitEvent can be instead managed by start stop quest management.
    // bind game event listeners
    auto* pEventList = EventDispatcherManager::Get();
    pEventList->questStartStopEvent.RegisterSink(this);
    pEventList->questStageEvent.RegisterSink(this);

    pEventList->scenePhaseEvent.RegisterSink(this);
    pEventList->sceneActionEvent.RegisterSink(this);
    pEventList->sceneEvent.RegisterSink(this);
}

void QuestService::OnConnected(const ConnectedEvent&) noexcept
{
    // TODO: this should be followed with whatever the quest leader selected
    /*
    // deselect any active quests
    auto* pPlayer = PlayerCharacter::Get();
    for (auto& objective : pPlayer->objectives)
    {
        if (auto* pQuest = objective.instance->quest)
            pQuest->SetActive(false);
    }
    */
}

BSTEventResult QuestService::OnEvent(const TESQuestStartStopEvent* apEvent, const EventDispatcher<TESQuestStartStopEvent>*)
{
    if (!m_world.Get().GetPartyService().IsInParty())
    {
        spdlog::debug("Not in party, quest stage advancement won't be sent");
        return BSTEventResult::kOk;
    }

    spdlog::info("Local OnEvent: quest start/stop event: {:X}", apEvent->formId);

    if (TESQuest* pQuest = Cast<TESQuest>(TESForm::GetById(apEvent->formId)))
    {
        if (IsNonSyncableQuest(pQuest))
            return BSTEventResult::kOk;
     
        if (pQuest->type == TESQuest::Type::None || pQuest->type == TESQuest::Type::Miscellaneous)
        {
            // Perhaps redundant, but necessary. We need the logging and
            // the lambda coming up is queued and runs later
            GameId Id;
            auto& modSys = m_world.GetModSystem();
            if (modSys.GetServerModId(pQuest->formID, Id))
            {
                spdlog::info(__FUNCTION__ ": queuing type none/misc quest gameId {:X} questStage {} questStatus {} questType {} formId {:X} name {}",
                             Id.LogFormat(),  pQuest->currentStage, pQuest->IsStopped() ? RequestQuestUpdate::Stopped : RequestQuestUpdate::Started,
                             static_cast<std::underlying_type_t<TESQuest::Type>>(pQuest->type), 
                             pQuest->formID, pQuest->fullName.value.AsAscii());
            }
        }
        
        m_world.GetRunner().Queue(
            [&, formId = pQuest->formID, stageId = pQuest->currentStage, stopped = pQuest->IsStopped(), type = pQuest->type]()
            {
                GameId Id;
                auto& modSys = m_world.GetModSystem();
                if (modSys.GetServerModId(formId, Id))
                {
                    RequestQuestUpdate update;
                    update.Id = Id;
                    update.Stage = stageId;
                    update.Status = stopped ? RequestQuestUpdate::Stopped : RequestQuestUpdate::Started;
                    update.ClientQuestType = static_cast<std::underlying_type_t<TESQuest::Type>>(type); 

                    m_world.GetTransport().Send(update);
                }
            });
    }

    return BSTEventResult::kOk;
}

BSTEventResult QuestService::OnEvent(const TESQuestStageEvent* apEvent, const EventDispatcher<TESQuestStageEvent>*)
{
    if (!CanAdvanceQuestForParty())
    {
        spdlog::warn("Quest stage advancement won't be sent: either not in party, or a non-leader with disabled controls.");
        return BSTEventResult::kOk;
    }

    spdlog::info("Local OnEvent: quest stage event: {:X}, stage: {}. Sending to server.", apEvent->formId, apEvent->stageId);

    // there is no reason to even fetch the quest object, since the event provides everything already....
    if (TESQuest* pQuest = Cast<TESQuest>(TESForm::GetById(apEvent->formId)))
    {
        if (IsNonSyncableQuest(pQuest))
            return BSTEventResult::kOk;

        if (pQuest->type == TESQuest::Type::None || pQuest->type == TESQuest::Type::Miscellaneous)
        {
            // Perhaps redundant, but necessary. We need the logging and
            // the lambda coming up is queued and runs later
            GameId Id;
            auto& modSys = m_world.GetModSystem();
            if (modSys.GetServerModId(pQuest->formID, Id))
            {
                spdlog::info(__FUNCTION__ ": queuing type none/misc quest gameId {:X} questStage {} questStatus {} questType {} formId {:X} name {}",
                             Id.LogFormat(), pQuest->currentStage,
                             RequestQuestUpdate::StageUpdate,
                             static_cast<std::underlying_type_t<TESQuest::Type>>(pQuest->type),
                             pQuest->formID, pQuest->fullName.value.AsAscii());
            }
        }

        //m_doneStages[apEvent->formId].push_back(apEvent->stageId);

        m_world.GetRunner().Queue(
            [&, formId = apEvent->formId, stageId = apEvent->stageId, type = pQuest->type]()
            {
                GameId Id;
                auto& modSys = m_world.GetModSystem();
                if (modSys.GetServerModId(formId, Id))
                {
                    RequestQuestUpdate update;
                    update.Id = Id;
                    update.Stage = stageId;
                    update.Status = RequestQuestUpdate::StageUpdate;
                    update.ClientQuestType = static_cast<std::underlying_type_t<TESQuest::Type>>(type);

                    m_world.GetTransport().Send(update);
                }
            });
    }

    return BSTEventResult::kOk;
}

BSTEventResult QuestService::OnEvent(const TESSceneEvent* apEvent, const EventDispatcher<TESSceneEvent>*)
{
    const String sceneType = apEvent->sceneType == 0 ? "Begin" : "End";
    spdlog::info("TESSceneEvent event: quest stage {}, type {}", apEvent->questStageId, sceneType);

    return BSTEventResult::kOk;
}

BSTEventResult QuestService::OnEvent(const TESSceneActionEvent* apEvent, const EventDispatcher<TESSceneActionEvent>*)
{
    spdlog::info("TESSceneActionEvent event: quest stage {}, ref alias {:X}", apEvent->questStageId, apEvent->refAliasId);
    return BSTEventResult::kOk;
}

BSTEventResult QuestService::OnEvent(const TESScenePhaseEvent* apEvent, const EventDispatcher<TESScenePhaseEvent>*)
{
    const String sceneType = apEvent->sceneType == 0 ? "Begin" : "End";
    spdlog::info("TESScenePhaseEvent event: quest stage {}, phase index {}, type {}", apEvent->questStageId, apEvent->phaseIndex, sceneType);
    return BSTEventResult::kOk;
}

void QuestService::OnQuestUpdate(const NotifyQuestUpdate& aUpdate) noexcept
{
    ModSystem& modSystem = World::Get().GetModSystem();
    uint32_t formId = modSystem.GetGameId(aUpdate.Id);
    TESQuest* pQuest = Cast<TESQuest>(TESForm::GetById(formId));
    if (!pQuest)
    {
        spdlog::error("Failed to find quest, base id: {:X}, mod id: {:X}", aUpdate.Id.BaseId, aUpdate.Id.ModId);
        return;
    }

    if (pQuest->type == TESQuest::Type::None || pQuest->type == TESQuest::Type::Miscellaneous)
    {
        spdlog::info(__FUNCTION__ ": receiving type none/misc quest update gameId {:X} questStage {} questStatus {} questType {} formId {:X} name {}",
                     aUpdate.Id.LogFormat(), aUpdate.Stage, aUpdate.Status,
                     aUpdate.ClientQuestType, formId, pQuest->fullName.value.AsAscii());
    }

    //const auto& doneStages = m_doneStages[pQuest->formID];
    //if (std::find(doneStages.begin(), doneStages.end(), aUpdate.Stage) != doneStages.end())
    //{
        // We've already completed this stage recently. Return to avoid reflection
        //return;
    //}

    bool bResult = false;
    switch (aUpdate.Status)
    {
    case NotifyQuestUpdate::Started:
    {
        spdlog::info("(NotifyQuestUpdate) Remote quest started. Starting local quest {:X}, stage: {}", formId, aUpdate.Stage);
        pQuest->ScriptSetStage(aUpdate.Stage);
        pQuest->SetActive(true);
        bResult = true;
        break;
    }
    case NotifyQuestUpdate::StageUpdate:
        spdlog::info("(NotifyQuestUpdate) Remote quest updated. Updating local quest {:X}, stage: {}", formId, aUpdate.Stage);
        pQuest->ScriptSetStage(aUpdate.Stage);
        bResult = true;
        break;
    case NotifyQuestUpdate::Stopped:
        spdlog::info("(NotifyQuestUpdate) Remote quest stopped. Stopping local quest {:X}, stage: {}", formId, aUpdate.Stage);
        bResult = StopQuest(formId);
        break;
    default: break;
    }

    if (!bResult)
        spdlog::error("Failed to update the client quest state, quest: {:X}, stage: {}, status: {}", formId, aUpdate.Stage, aUpdate.Status);
}

bool QuestService::CanAdvanceQuestForParty() const noexcept
{
    const bool isInParty = m_world.Get().GetPartyService().IsInParty();
    // Party leaders can always advance quests. 
    // Members can only advance quest stages when their controls are enabled (needed for scripted cutscenes to work properly)
    const bool canAdvanceQuestStages = m_world.Get().GetPartyService().IsLeader() || PlayerControls::IsMovementControlsEnabled();

    return isInParty && canAdvanceQuestStages;
}

bool QuestService::StopQuest(uint32_t aformId)
{
    TESQuest* pQuest = Cast<TESQuest>(TESForm::GetById(aformId));
    if (pQuest)
    {
        pQuest->SetActive(false);
        pQuest->SetStopped();
        return true;
    }

    return false;
}

static constexpr std::array kNonSyncableQuestIds = std::to_array<uint32_t>({
    0x2BA16,   // Werewolf transformation quest
    0x20071D0, // Vampire transformation quest
    0x3AC44,   // MS13BleakFallsBarrowLeverScene
    // 0xFE014801,  // Unknown dynamic ID, kept as note, maybe lookup correct ID this game?
    0xF2593 // Skill experience quest
});

bool QuestService::IsNonSyncableQuest(TESQuest* apQuest)
{
    // Quests with no quest stages are never synced. Most TESQues::Type:: quests should
    // be synced, including Type::None and Type::Miscellaneous, but there are a few
    // known exceptions that should be excluded that are in the table.
    return    apQuest->stages.Empty() 
           || std::find(kNonSyncableQuestIds.begin(), kNonSyncableQuestIds.end(), apQuest->formID) != kNonSyncableQuestIds.end();
}

void QuestService::DebugDumpQuests()
{
    auto& quests = ModManager::Get()->quests;
    for (TESQuest* pQuest : quests)
        spdlog::info("{:X}|{}|{}|{}", pQuest->formID, (uint8_t)pQuest->type, pQuest->priority, pQuest->idName.AsAscii());
}
