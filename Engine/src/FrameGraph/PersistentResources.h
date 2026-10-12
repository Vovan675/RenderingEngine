#pragma once
#include "FrameGraphRHIResources.h"

// Keyed by exact resource name, same resource every frame. Content may be used from previous frame, not reusable.
class PersistentResources
{
public:
	void deleteUndeclared();

	bool declareTexture(GraphicsResourceName name, const TextureDescription &desc);
	bool declareBuffer(GraphicsResourceName name, const BufferDescription &desc);

	RHITexture *getTexture(GraphicsResourceName name, const TextureDescription &desc, RHICommandList *cmd_list);
	RHIBuffer *getBuffer(GraphicsResourceName name, const BufferDescription &desc, RHICommandList *cmd_list);

private:
	struct PersistentTexture
	{
		RHITextureRef texture;
		TextureDescription declared_desc;
		uint64_t last_declared_frame = 0;
	};

	struct PersistentBuffer
	{
		RHIBufferRef buffer;
		BufferDescription declared_desc;
		uint64_t last_declared_frame = 0;
	};

	eastl::unordered_map<GraphicsResourceName, PersistentTexture> textures;
	eastl::unordered_map<GraphicsResourceName, PersistentBuffer> buffers;
};
