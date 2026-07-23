
#include "textured_model_sample.h"

using namespace PHX;

int main(int argc, char** argv)
{
	(void)argc;
	(void)argv;

	TexturedModelSample sample;
	sample.Init();

	while(!sample.Update(0.016f))
	{
		sample.Draw();
	}

	sample.Shutdown();
	return 0;
}