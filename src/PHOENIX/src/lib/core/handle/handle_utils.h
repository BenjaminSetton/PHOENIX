#pragma once

#include <vector>

#include "PHX/interface/acceleration_structure.h"
#include "PHX/interface/buffer.h"
#include "PHX/interface/device_context.h"
#include "PHX/interface/render_device.h"
#include "PHX/interface/render_graph.h"
#include "PHX/interface/shader.h"
#include "PHX/interface/swap_chain.h"
#include "PHX/interface/texture.h"
#include "PHX/interface/uniform.h"
#include "PHX/interface/window.h"

#include "core/handle/handle_accessor.h"
#include "core/handle/handle_list.h"

namespace PHX
{
	// Forward declarations
	class ITexture;
	class IBuffer;
	class IUniformCollection;
	class IDeviceContext;
	class IRenderGraph;
	class IRenderPass;
	class IShader;
	class ISwapChain;
	class IRenderDevice;
	class IWindow;
	class IAccelerationStructure;

	namespace HANDLE_UTILS
	{
		ITexture* ResolveHandle(const TextureHandle& handle);
		IBuffer* ResolveHandle(const BufferHandle& handle);
		IUniformCollection* ResolveHandle(const UniformCollectionHandle& handle);
		IDeviceContext* ResolveHandle(const DeviceContextHandle& handle);
		IRenderGraph* ResolveHandle(const RenderGraphHandle& handle);
		IRenderPass* ResolveHandle(const RenderPassHandle& handle);
		IShader* ResolveHandle(const ShaderHandle& handle);
		ISwapChain* ResolveHandle(const SwapChainHandle& handle);
		IRenderDevice* ResolveHandle(const RenderDeviceHandle& handle);
		IWindow* ResolveHandle(const WindowHandle& handle);
		IAccelerationStructure* ResolveHandle(const AccelerationStructureHandle& handle);

		///////////////////////////////////
		// ALLOCATE HANDLE
		///////////////////////////////////

		template<typename InterfaceT>
		STATUS_CODE AllocateHandle(HandleList<InterfaceT>& list, InterfaceT* pObj, HandleOwner* pOwner, Handle& handle)
		{
			const u32 index = list.Allocate(pObj);
			HandleAccessor::PopulateHandle(handle, pOwner, index);
			return STATUS_CODE::SUCCESS;
		}
	}
}