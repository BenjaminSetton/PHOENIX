#pragma once

#include <vector>

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
	//class IWindow;

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
		//IWindow* ResolveHandle(const WindowHandle& handle);

		template<typename InterfaceT>
		void* ResolveHandle(const std::vector<InterfaceT*>& list, const Handle& handle)
		{
			const u32 index = HandleAccessor::GetIndex(handle);
			if (index >= static_cast<u32>(list.size()))
			{
				return list[index];
			}
			return nullptr;
		}

		// Used to allocate objects into lists
		template<typename DerivedInterfaceT>
		STATUS_CODE AllocateHandle(std::vector<DerivedInterfaceT*>& list, DerivedInterfaceT* pObj, HandleOwner* pOwner, const Handle& handle)
		{
			list.push_back(pObj);
			pObj->IncrementRefCount();

			HandleAccessor::PopulateHandle(handle, pOwner, static_cast<u32>(list.size() - 1), 0u);
			return STATUS_CODE::SUCCESS;
		}

		// Used to allocate objects when only a single instance can exist
		template<typename DerivedInterfaceT>
		STATUS_CODE AllocateHandle(DerivedInterfaceT* pObj, HandleOwner* pOwner, const Handle& handle)
		{
			pObj->IncrementRefCount();

			HandleAccessor::PopulateHandle(handle, pOwner, 0u, 0u);
			return STATUS_CODE::SUCCESS;
		}

		template<typename HandleT, typename InterfaceT>
		void IncrementRefCount(const Handle& handle)
		{
			InterfaceT* pObj = ResolveHandle(static_cast<const HandleT&>(handle));
			if (pObj != nullptr)
			{
				pObj->IncrementRefCount();
			}
		}

		template<typename HandleT, typename DerivedInterfaceT>
		void DecrementRefCount(const Handle& handle, std::vector<DerivedInterfaceT*>& list)
		{
			DerivedInterfaceT* pObj = static_cast<DerivedInterfaceT*>(ResolveHandle(static_cast<const HandleT&>(handle)));
			if (pObj != nullptr)
			{
				pObj->DecrementRefCount();

				// Check for deletion
				if (pObj->GetRefCount() <= 0)
				{
					// Assuming generation is always 0
					const u32 index = HandleAccessor::GetIndex(handle);
					if (index <= static_cast<u32>(list.size()))
					{
						DerivedInterfaceT* pObj = list[index];
						SAFE_DEL(pObj);

						list.erase(list.begin() + index);
					}
				}
			}
		}
	}
}