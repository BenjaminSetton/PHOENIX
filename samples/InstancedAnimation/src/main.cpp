
#include "instanced_animation_sample.h"

using namespace PHX;

int main(int argc, char** argv)
{
	(void)argc;
	(void)argv;

	InstancedAnimationSample sample;
	sample.Init();

	while (!sample.Update(0.016f))
	{
		sample.Draw();
	}

	sample.Shutdown();
	return 0;
}
