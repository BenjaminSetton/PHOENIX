
#include "core/handle/handle_utils.h"
#include "core/interface_types/swap_chain_interface.h"
#include "PHX/interface/render_device.h"
#include "utils/sanity.h"

namespace PHX
{
	DEFINE_PHX_HANDLE(SwapChainHandle, HANDLE_TYPE::SWAP_CHAIN)

	TextureHandle SwapChainHandle::GetCurrentImage() const
	{
		ISwapChain* pSwapChain = HANDLE_UTILS::ResolveHandle(*this);
		if (pSwapChain != nullptr)
		{
			return pSwapChain->GetCurrentImage();
		}

		ASSERT_ALWAYS("Failed to get current image. Could not resolve swap chain handle!");
		return INVALID_HANDLE;
	}

	u32 SwapChainHandle::GetImageCount() const
	{
		ISwapChain* pSwapChain = HANDLE_UTILS::ResolveHandle(*this);
		if (pSwapChain != nullptr)
		{
			return pSwapChain->GetImageCount();
		}

		ASSERT_ALWAYS("Failed to get image count. Could not resolve swap chain handle!");
		return 0;
	}

	u32 SwapChainHandle::GetCurrentImageIndex() const
	{
		ISwapChain* pSwapChain = HANDLE_UTILS::ResolveHandle(*this);
		if (pSwapChain != nullptr)
		{
			return pSwapChain->GetCurrentImageIndex();
		}

		ASSERT_ALWAYS("Failed to get current image index. Could not resolve swap chain handle!");
		return 0;
	}

	void SwapChainHandle::Resize(u32 newWidth, u32 newHeight)
	{
		ISwapChain* pSwapChain = HANDLE_UTILS::ResolveHandle(*this);
		if (pSwapChain != nullptr)
		{
			return pSwapChain->Resize(newWidth, newHeight);
		}
	}

	u32 SwapChainHandle::GetWidth() const
	{
		ISwapChain* pSwapChain = HANDLE_UTILS::ResolveHandle(*this);
		if (pSwapChain != nullptr)
		{
			return pSwapChain->GetWidth();
		}

		ASSERT_ALWAYS("Failed to get width. Could not resolve swap chain handle!");
		return 0;
	}

	u32 SwapChainHandle::GetHeight() const
	{
		ISwapChain* pSwapChain = HANDLE_UTILS::ResolveHandle(*this);
		if (pSwapChain != nullptr)
		{
			return pSwapChain->GetHeight();
		}

		ASSERT_ALWAYS("Failed to get height. Could not resolve swap chain handle!");
		return 0;
	}
}