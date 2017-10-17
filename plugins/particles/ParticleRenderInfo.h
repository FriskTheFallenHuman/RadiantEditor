#pragma once

#include "math/Vector3.h"
#include "math/Vector4.h"
#include <random>

// The randomizer typedef
// We used to have boost::rand48 here, this is its replacement using the C++11 LCG
// The constants below were copied directly from the boost headers
typedef std::linear_congruential_engine<std::uint_fast64_t,
	uint64_t(0xDEECE66DUL) | (uint64_t(0x5) << 32), 0xB, uint64_t(1) << 48> Rand48;

namespace particles
{

/**
 * A structure holding info about how to draw a certain particle,
 * including texcoords, fade colour, etc.
 *
 * This info can apply to a single ParticleQuad or a whole quad group
 * if the particle stage is animated or aimed.
 */
struct ParticleRenderInfo
{
	std::size_t index;	// zero-based index of this particle within a stage

	float timeSecs;		// time in seconds
	float timeFraction;	// time fraction within particle lifetime

	Vector3 origin;
	Vector4 colour;		// resulting colour

	float angle;		// the angle of the quad
	float size;			// the desired size (might be overridden when aimed)
	float aspect;		// the desired aspect ratio (might be overridden when aimed)

	float sWidth;		// the horizontal amount of texture space occupied by this particle (for anims)
	float t0;			// Vertical texture coordinate
	float tWidth;		// the vertical amount of texture space occupied by this particle (for aiming)

	float rand[5];		// 5 random numbers needed for pathing

	std::size_t animFrames; // animation: number of frames (0 if not animated)
	std::size_t curFrame;	// animation: current frame
	std::size_t nextFrame;	// animation: next frame

	Vector4 curColour;
	Vector4 nextColour;

	ParticleRenderInfo() :
		index(0),
		angle(0),
		sWidth(1),
		t0(0),
		tWidth(1)
	{}

	// Construct a particle with the given index, and fill in the random variables
	ParticleRenderInfo(std::size_t index_, Rand48& random) :
		index(index_),
		angle(0),
		sWidth(1),
		t0(0),
		tWidth(1)
	{
		Rand48::result_type maxVal = random.max();

		// Generate five random numbers for path calcs, this is needed in calculateOrigin
		rand[0] = static_cast<float>(random()) / maxVal;
		rand[1] = static_cast<float>(random()) / maxVal;
		rand[2] = static_cast<float>(random()) / maxVal;
		rand[3] = static_cast<float>(random()) / maxVal;
		rand[4] = static_cast<float>(random()) / maxVal;
	}
};

} // namespace
