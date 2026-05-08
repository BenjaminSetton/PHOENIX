
#include "compute_particles_sample.h"

using namespace PHX;

int main(int argc, char** argv)
{
	(void)argc;
	(void)argv;

	ComputeParticlesSample sample;
	while (!sample.Update(0.016f))
	{
		sample.Draw();
	}

	return 0;
}