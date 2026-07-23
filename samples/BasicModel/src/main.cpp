
#include "basic_model_sample.h"

int main(int argc, char** argv)
{
	(void)argc;
	(void)argv;

	BasicModelSample sample;
	sample.Init();

	while (!sample.Update(0.016f))
	{
		sample.Draw();
	}

	sample.Shutdown();
	return 0;
}