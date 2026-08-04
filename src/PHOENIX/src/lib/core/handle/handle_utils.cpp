
#include "BSL/logger.h"
#include "core/handle/handle_accessor.h"
#include "core/handle/handle_owner.h"
#include "core/handle/handle_utils.h"

#define RESOLVE_HELPER(handle, HandleT) \
	const bool canResolve = handle.IsValid();                          \
	if(canResolve) {                                                   \
		HandleOwner* pOwner = HandleAccessor::GetOwner(handle);        \
		return static_cast<HandleT*>(pOwner->ResolveHandle(handle));   \
	}                                                                  \
	else {                                                             \
		BSL::LogError("Failed to resolve handle. Handle is invalid!"); \
		return nullptr;                                                \
	}

namespace PHX
{
	namespace HANDLE_UTILS
	{
		ITexture* ResolveHandle(const TextureHandle& handle)
		{
			RESOLVE_HELPER(handle, ITexture);
		}

		IBuffer* ResolveHandle(const BufferHandle& handle)
		{
			RESOLVE_HELPER(handle, IBuffer);
		}

		IUniformCollection* ResolveHandle(const UniformCollectionHandle& handle)
		{
			RESOLVE_HELPER(handle, IUniformCollection);
		}

		IDeviceContext* ResolveHandle(const DeviceContextHandle& handle)
		{
			RESOLVE_HELPER(handle, IDeviceContext);
		}

		IRenderGraph* ResolveHandle(const RenderGraphHandle& handle)
		{
			RESOLVE_HELPER(handle, IRenderGraph);
		}

		IRenderPass* ResolveHandle(const RenderPassHandle& handle)
		{
			RESOLVE_HELPER(handle, IRenderPass);
		}

		IShader* ResolveHandle(const ShaderHandle& handle)
		{
			RESOLVE_HELPER(handle, IShader);
		}

		ISwapChain* ResolveHandle(const SwapChainHandle& handle)
		{
			RESOLVE_HELPER(handle, ISwapChain);
		}

		IRenderDevice* ResolveHandle(const RenderDeviceHandle& handle)
		{
			RESOLVE_HELPER(handle, IRenderDevice);
		}

		IWindow* ResolveHandle(const WindowHandle& handle)
		{
			RESOLVE_HELPER(handle, IWindow);
		}

		IAccelerationStructure* ResolveHandle(const AccelerationStructureHandle& handle)
		{
			RESOLVE_HELPER(handle, IAccelerationStructure);
		}
	}
}