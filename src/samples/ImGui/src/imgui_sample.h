#pragma once

#include "../../common/src/base_sample.h"

class ImGuiSample : public Common::BaseSample
{
public:

	ImGuiSample();
	~ImGuiSample() override;

	void Draw() override;

protected:

	void InitSample() override;
	void ShutdownSample() override;
	void UpdateSample(float dt) override;
};
