
#include "textured_model_sample.h"

using namespace PHX;

int main(int argc, char** argv)
{
	(void)argc;
	(void)argv;

	TexturedModelSample sample;
	while(!sample.Update(0.0f))
	{
		sample.Draw();
	}

	return 0;
}