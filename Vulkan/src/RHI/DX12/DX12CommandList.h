#pragma once
#include "RHI/DynamicRHI.h"

class DX12CommandList: public RHICommandList
{
public:
	DX12CommandList(ComPtr<ID3D12Device> device, D3D12_COMMAND_LIST_TYPE type)
	{
		device->CreateCommandAllocator(type, IID_PPV_ARGS(&cmd_allocator));
		device->CreateCommandList(0, type, cmd_allocator.Get(), nullptr, IID_PPV_ARGS(&cmd_list));
		cmd_list->Close();
	}

	~DX12CommandList()
	{
		cmd_allocator.Reset();
		cmd_list.Reset();
	}

	void open() override
	{
		cmd_allocator->Reset();
		cmd_list->Reset(cmd_allocator.Get(), nullptr);
	}

	void close() override
	{
		cmd_list->Close();
	}

	void setRenderTargets(const std::vector<std::shared_ptr<RHITexture>> &color_attachments, std::shared_ptr<RHITexture> depth_attachment, int layer, int mip, bool clear) override;

	void resetRenderTargets() override
	{
		current_render_targets.clear();
	}

	std::vector<std::shared_ptr<RHITexture>> &getCurrentRenderTargets()
	{
		return current_render_targets;
	}

	void setPipeline(std::shared_ptr<RHIPipeline> pipeline) override;

	void setVertexBuffer(std::shared_ptr<RHIBuffer> buffer) override;

	void setIndexBuffer(std::shared_ptr<RHIBuffer> buffer) override;

	void drawIndexedInstanced(uint32_t indexCount, uint32_t instanceCount, uint32_t firstIndex, int32_t vertexOffset, uint32_t firstInstance) override
	{
		gDynamicRHI->prepareRenderCall();
		cmd_list->DrawIndexedInstanced(indexCount, instanceCount, firstIndex, vertexOffset, firstInstance);
	}

	void drawInstanced(uint32_t vertex_count_per_instance, uint32_t instance_count, uint32_t firstVertex, uint32_t firstInstance)
	{
		gDynamicRHI->prepareRenderCall();
		cmd_list->DrawInstanced(vertex_count_per_instance, instance_count, firstVertex, firstInstance);
	}

	void dispatch(uint32_t group_x, uint32_t group_y, uint32_t group_z) override
	{
		gDynamicRHI->prepareRenderCall();
		cmd_list->Dispatch(group_x, group_y, group_z);
	}

	void beginDebugLabel(const char *label, glm::vec3 color)
	{
		// TODO: integrate pix events
	}

	void endDebugLabel()
	{
		// TODO: integrate pix events
	}

	ComPtr<ID3D12CommandAllocator> cmd_allocator;
	ComPtr<ID3D12GraphicsCommandList> cmd_list;
	std::shared_ptr<RHIPipeline> current_pipeline;
	std::vector<std::shared_ptr<RHITexture>> current_render_targets;
};