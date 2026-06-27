
#include "PHX/interface/render_device.h"
#include "PHX/interface/swap_chain.h"

#include "core/handle/handle_utils.h"
#include "utils/sanity.h"

namespace PHX
{
	SwapChainHandle::SwapChainHandle() : Handle(HANDLE_TYPE::SWAP_CHAIN)
	{
	}

	SwapChainHandle::SwapChainHandle(const Handle& base) : Handle(base)
	{
	}

	SwapChainHandle::~SwapChainHandle()
	{
	}

	SwapChainHandle::SwapChainHandle(const SwapChainHandle& other) : Handle(other)
	{
	}

	SwapChainHandle& SwapChainHandle::operator=(const SwapChainHandle& other)
	{
		if (this == &other)
		{
			return *this;
		}

		Handle::operator=(other);
		return *this;
	}

	SwapChainHandle::SwapChainHandle(SwapChainHandle&& other) noexcept : Handle(std::move(other))
	{
	}

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

	STATUS_CODE SwapChainHandle::Present()
	{
		ISwapChain* pSwapChain = HANDLE_UTILS::ResolveHandle(*this);
		if (pSwapChain != nullptr)
		{
			return pSwapChain->Present();
		}

		ASSERT_ALWAYS("Failed to present. Could not resolve swap chain handle!");
		return STATUS_CODE::ERR_INTERNAL;
	}

	void SwapChainHandle::Resize(u32 newWidth, u32 newHeight)
	{
		ISwapChain* pSwapChain = HANDLE_UTILS::ResolveHandle(*this);
		if (pSwapChain != nullptr)
		{
			return pSwapChain->Resize(newWidth, newHeight);
		}
	}
}