#pragma once

#include "Message.h"
#include <Structs/GameId.h>

struct NotifyQuestSceneUpdate final : ServerMessage
{
    static constexpr ServerOpcode Opcode = kNotifyQuestSceneUpdate;

    NotifyQuestSceneUpdate()
        : ServerMessage(Opcode)
    {
    }

    void SerializeRaw(TiltedPhoques::Buffer::Writer& aWriter) const noexcept override;
    void DeserializeRaw(TiltedPhoques::Buffer::Reader& aReader) noexcept override;

    bool operator==(const NotifyQuestSceneUpdate& acRhs) const noexcept { return GetOpcode() == acRhs.GetOpcode() && SceneId == acRhs.SceneId && QuestId == acRhs.QuestId; }

    GameId SceneId;
    GameId QuestId;
};
