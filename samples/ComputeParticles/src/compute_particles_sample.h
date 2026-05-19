#pragma once

#include "../../common/src/base_sample.h"

struct Particle
{
	// TODO - Add glm dependency
};

class ComputeParticlesSample : public Common::BaseSample
{
public:

	ComputeParticlesSample();
	~ComputeParticlesSample();

	void Draw() override;

private:

	void Init() override;
	void Shutdown() override;

private:

	std::vector<Particle> m_particles;
};