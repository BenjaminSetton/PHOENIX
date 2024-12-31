
#include "PHX/phx.h"

#include "utils/object_factory.h"
#include "utils/sanity.h"
#include "utils/logger.h"

static struct GlobalGraphicsData
{
	bool m_isInitialized = false;
	PHX::GRAPHICS_API m_selectedAPI;
	std::shared_ptr<PHX::IWindow> m_pWindow;
	std::shared_ptr<PHX::IRenderDevice> m_pRenderDevice;
	std::shared_ptr<PHX::ISwapChain> m_pSwapChain;
} g_Data;

namespace PHX
{
	STATUS_CODE CreateWindow(const WindowCreateInfo& createInfo)
	{
		g_Data.m_pWindow = OBJ_FACTORY::CreateWindow(createInfo);

		// TODO - Figure out a way to capture errors. Catch exceptions or change OBJ_FACTORY return type?
		return STATUS_CODE::SUCCESS;
	}

	STATUS_CODE InitializeGraphics(GRAPHICS_API api)
	{
		if (g_Data.m_pWindow.get() == nullptr)
		{
			LogError("Failed to initialize graphics. Window hasn't been initialized yet!");
			return STATUS_CODE::ERR;
		}

		STATUS_CODE coreObjStatus = OBJ_FACTORY::CreateCoreObjects(api, g_Data.m_pWindow);
		if (coreObjStatus != STATUS_CODE::SUCCESS)
		{
			return coreObjStatus;
		}

		g_Data.m_isInitialized = true;
		g_Data.m_selectedAPI = api;

		return STATUS_CODE::SUCCESS;
	}

	STATUS_CODE CreateRenderDevice(const RenderDeviceCreateInfo& createInfo)
	{
		if (!g_Data.m_isInitialized)
		{
			LogError("Failed to create render device. Graphics has not been initialized through InitializeGraphics()!");
			return STATUS_CODE::ERR;
		}

		auto pRenderDevice = OBJ_FACTORY::CreateRenderDevice(createInfo, g_Data.m_selectedAPI);
		g_Data.m_pRenderDevice = pRenderDevice;

		// TODO - Figure out a way to capture errors. Catch exceptions or change OBJ_FACTORY return type?
		return STATUS_CODE::SUCCESS;
	}

	STATUS_CODE CreateSwapChain(const SwapChainCreateInfo& createInfo)
	{
		if (!g_Data.m_isInitialized)
		{
			LogError("Failed to create swap chain. Graphics has not been initialized through InitializeGraphics()!");
			return STATUS_CODE::ERR;
		}

		auto pSwapChain = OBJ_FACTORY::CreateSwapChain(createInfo, g_Data.m_selectedAPI);
		g_Data.m_pSwapChain = pSwapChain;

		// TODO - Figure out a way to capture errors. Catch exceptions or change OBJ_FACTORY return type?
		return STATUS_CODE::SUCCESS;
	}

	std::shared_ptr<IWindow> GetWindow()
	{
		return g_Data.m_pWindow;
	}

	std::shared_ptr<IRenderDevice> GetRenderDevice()
	{
		return g_Data.m_pRenderDevice;
	}

	std::shared_ptr<ISwapChain> GetSwapChain()
	{
		return g_Data.m_pSwapChain;
	}

	STATUS_CODE CompileShader(const char* shaderSrc, u32 numBytes, SHADER_TYPE type)
	{
		return STATUS_CODE::SUCCESS;
	}
}