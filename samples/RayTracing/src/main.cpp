
#include "ray_tracing_sample.h"

int main(int argc, char** argv)
{
	(void)argc;
	(void)argv;

	RayTracingSample sample;
	while (!sample.Update(0.016f))
	{
		sample.Draw();
	}

	return 0;
}
