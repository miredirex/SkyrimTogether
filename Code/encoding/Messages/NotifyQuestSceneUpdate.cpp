
#include <Messages/NotifyQuestSceneUpdate.h>
#include <TiltedCore/Serialization.hpp>

void NotifyQuestSceneUpdate::SerializeRaw(TiltedPhoques::Buffer::Writer& aWriter) const noexcept
{
    SceneId.Serialize(aWriter);
    QuestId.Serialize(aWriter);
}

void NotifyQuestSceneUpdate::DeserializeRaw(TiltedPhoques::Buffer::Reader& aReader) noexcept
{
    ServerMessage::DeserializeRaw(aReader);
    SceneId.Deserialize(aReader);
    QuestId.Deserialize(aReader);
}
