#include "pch.h"
#include "DX12CommandList.h"
#include "DX12DynamicRHI.h"
#include "RHI/DynamicRHI.h"
#include "RHI/RHITexture.h"
#include "RHI/RHIPipeline.h"

void DX12CommandList::setRenderTargets(const std::vector<std::shared_ptr<RHITexture>> &color_attachments, std::shared_ptr<RHITexture> depth_attachment, int layer, int mip, bool clear)
{
	D3D12_VIEWPORT viewport;
	D3D12_RECT surface_rect;
	surface_rect.left = 0;
	surface_rect.top = 0;

	if (color_attachments.size() > 0)
	{
		surface_rect.right = color_attachments[0]->getWidth(mip);
		surface_rect.bottom = color_attachments[0]->getHeight(mip);
	} else
	{
		surface_rect.right = depth_attachment->getWidth(mip);
		surface_rect.bottom = depth_attachment->getHeight(mip);
	}


	viewport.TopLeftX = 0.0f;
	viewport.TopLeftY = 0.0f;
	viewport.Width = surface_rect.right;
	viewport.Height = surface_rect.bottom;
	viewport.MinDepth = 0.0f;
	viewport.MaxDepth = 1.0f;


	const float clearColor[] = {0.0f, 0.0f, 0.0f, 1.0f};

	std::vector<D3D12_CPU_DESCRIPTOR_HANDLE> rtvs;
	for (const auto &attachment : color_attachments)
	{
		DX12Texture *texture = (DX12Texture *)attachment.get();
		rtvs.push_back(texture->getRenderTargetView(mip, layer));

		if (clear)
			cmd_list->ClearRenderTargetView(texture->render_target_view, clearColor, 1, &surface_rect);
	}

	D3D12_CPU_DESCRIPTOR_HANDLE *depth_stencil = nullptr;
	if (depth_attachment)
	{
		DX12Texture *depth = (DX12Texture *)depth_attachment.get();
		depth_stencil = &depth->getDepthStencilView(mip, layer);
	}

	if (clear && depth_stencil)
		cmd_list->ClearDepthStencilView(*depth_stencil, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 1, &surface_rect);

	cmd_list->RSSetViewports(1, &viewport);
	cmd_list->RSSetScissorRects(1, &surface_rect);

	cmd_list->OMSetRenderTargets(rtvs.size(), rtvs.data(), FALSE, depth_stencil);

	current_render_targets = color_attachments;
	if (depth_attachment != nullptr)
		current_render_targets.push_back(depth_attachment);
}

void DX12CommandList::setPipeline(std::shared_ptr<RHIPipeline> pipeline)
{
	DX12Pipeline *native_pipeline = static_cast<DX12Pipeline *>(pipeline.get());

	cmd_list->SetPipelineState(native_pipeline->pipeline.Get());

	if (native_pipeline->description.is_ray_tracing_pipeline)
		cmd_list->SetComputeRootSignature(native_pipeline->root_signature.Get());
	else if (native_pipeline->description.is_compute_pipeline)
		cmd_list->SetComputeRootSignature(native_pipeline->root_signature.Get());
	else
		cmd_list->SetGraphicsRootSignature(native_pipeline->root_signature.Get());

	D3D_PRIMITIVE_TOPOLOGY topology = D3D_PRIMITIVE_TOPOLOGY_UNDEFINED;
	switch (native_pipeline->description.primitive_topology)
	{
		case TOPOLOGY_POINT_LIST: topology = D3D_PRIMITIVE_TOPOLOGY_POINTLIST; break;
		case TOPOLOGY_LINE_LIST: topology = D3D_PRIMITIVE_TOPOLOGY_LINELIST; break;
		case TOPOLOGY_TRIANGLE_LIST: topology = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST; break;
		case TOPOLOGY_TRIANGLE_STRIP: topology = D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP; break;
	}
	cmd_list->IASetPrimitiveTopology(topology);
	current_pipeline = pipeline;
}

void DX12CommandList::setVertexBuffer(std::shared_ptr<RHIBuffer> buffer)
{
	DX12Buffer *native_buffer = static_cast<DX12Buffer *>(buffer.get());
	cmd_list->IASetVertexBuffers(0, 1, &native_buffer->getVertexBufferView());
}

void DX12CommandList::setIndexBuffer(std::shared_ptr<RHIBuffer> buffer)
{
	DX12Buffer *native_buffer = static_cast<DX12Buffer *>(buffer.get());
	cmd_list->IASetIndexBuffer(&native_buffer->getIndexBufferView());
}
