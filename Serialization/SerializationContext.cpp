#include "SerializationContext.h"
#include "../Graphics/Assets/AssetLoader.h"

AssetLoader& LoadContext::GetAssetLoader() const
{
	return assets ? *assets : AssetLoader::Default();
}
