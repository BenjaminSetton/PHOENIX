
#include "ray_tracing_sample.h"

int main(int argc, char** argv)
{
	(void)argc;
	(void)argv;

	RayTracingSample sample;
	sample.Init();

	while (!sample.Update(0.016f))
	{
		sample.Draw();
	}

	sample.Shutdown();
	return 0;
}
