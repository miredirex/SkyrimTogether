
#include <Messages/RequestQuestSceneUpdate.h>
#include <TiltedCore/Serialization.hpp>

void RequestQuestSceneUpdate::SerializeRaw(TiltedPhoques::Buffer::Writer& aWriter) const noexcept
{
    SceneId.Serialize(aWriter);
    QuestId.Serialize(aWriter);
}

void RequestQuestSceneUpdate::DeserializeRaw(TiltedPhoques::Buffer::Reader& aReader) noexcept
{
    ClientMessage::DeserializeRaw(aReader);
    SceneId.Deserialize(aReader);
    QuestId.Deserialize(aReader);
}
