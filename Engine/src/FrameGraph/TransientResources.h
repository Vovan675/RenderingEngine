#pragma once
#include "RHI/RHITexture.h"
#include "RHI/RHIBuffer.h"

// Keyed by description, same resource may be reused inside one frame for different purposes.
// TODO: suballocate from same memory, aka alias it
class TransientResources
{
public:
	void deleteUnused();

	RHITextureRef getTexture(const TextureDescription &desc);
	void releaseTexture(RHITextureRef texture);

	RHIBufferRef getBuffer(const BufferDescription &desc);
	void releaseBuffer(RHIBufferRef buffer);

private:
	struct TransientTexture
	{
		RHITextureRef texture;
		uint64_t last_frame = 0;
	};

	struct TransientBuffer
	{
		RHIBufferRef buffer;
		uint64_t last_frame = 0;
	};

	eastl::unordered_map<size_t, eastl::vector<TransientTexture>> textures;
	eastl::unordered_map<size_t, eastl::vector<TransientBuffer>> buffers;
};
