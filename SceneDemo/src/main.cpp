
#include <iostream>

#include <PHX/phx.h>

int main(int argc, char** argv)
{
	using namespace PHX;
	(void)argc;
	(void)argv;

	ObjectFactory& factory = ObjectFactory::Get();
	factory.SelectGraphicsAPI(PHX::GRAPHICS_API::VULKAN);

	WindowCreateInfo windowCI{};
	std::shared_ptr<IWindow> pWindow = factory.CreateWindow(windowCI);

	RenderDeviceCreateInfo renderDeviceCI{};
	std::shared_ptr<IRenderDevice> pRenderDevice = factory.CreateRenderDevice(renderDeviceCI);
	pRenderDevice->AllocateBuffer();

	int i = 0;
	while (!pWindow->ShouldClose())
	{
		pWindow->Update(0.13f);
		pWindow->SetWindowTitle("PHX - %u", i++);
	}
}