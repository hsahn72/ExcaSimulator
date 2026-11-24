#pragma once

#include "CoreMinimal.h"

// Boundary between the excavator's physics and everything the player sees.
//
// AExcavatorPawn owns one IExcavatorPhysics. Each frame it sends the stick
// commands and what the bucket is touching, advances the model, then reads
// back joint angles and base motion and poses the visual parts from them.
// Nothing on the far side of this interface knows about actors or meshes, so
// the backend can be swapped without touching rendering, input or terrain:
//   - FKinematicExcavatorPhysics: the stage 1 model (rate-limited joints).
//   - Later: the hydraulic model from the HTML reference, or a Chrono model
//     running on its own thread at a fixed step.
//
// The soil shape stays in ADigTerrain (UE side). The backend only reports the
// force the soil puts on the bucket; it never edits the terrain.
//
// Units: degrees, deg/s, cm, cm/s, cm^3 (UE conventions). Forces in newtons.

// Operator commands, each in [-1, 1].
struct FExcavatorCommand
{
	float Swing = 0.f;       // + = right
	float Boom = 0.f;        // + = up
	float Arm = 0.f;         // + = out (push)
	float Bucket = 0.f;      // + = dump (open)
	float LeftTrack = 0.f;   // + = forward
	float RightTrack = 0.f;  // + = forward
};

// Joint angles relative to the parent link, and their rates.
struct FExcavatorJointState
{
	float SwingDeg = 0.f;
	float BoomDeg = 20.f;
	float ArmDeg = -110.f;
	float BucketDeg = -70.f;

	float SwingRate = 0.f;
	float BoomRate = 0.f;
	float ArmRate = 0.f;
	float BucketRate = 0.f;
};

// What the bucket is touching, measured by the UE side each frame.
struct FBucketSoilProbe
{
	FVector TipLocation = FVector::ZeroVector;   // cutting edge, world
	FVector TipVelocity = FVector::ZeroVector;   // world, cm/s
	FVector CutDirection = FVector::ForwardVector; // bucket X axis, world (pin -> edge)
	float SoilSurfaceZ = 0.f;                    // terrain height under the tip, world
	float CutDepth = 0.f;                        // max(0, SoilSurfaceZ - TipLocation.Z)
	float BucketLoadCm3 = 0.f;                   // soil currently carried
	bool bInSoil = false;
};

// Result of the latest step.
struct FExcavatorPhysicsState
{
	FExcavatorJointState Joints;

	// Undercarriage motion in its own frame. The pawn moves the actor and
	// keeps it on the terrain.
	float ForwardSpeed = 0.f;  // cm/s
	float YawRate = 0.f;       // deg/s

	// Force the soil applies to the bucket edge, world frame. Zero when the
	// backend does not model digging resistance.
	FVector BucketResistance = FVector::ZeroVector;
};

class IExcavatorPhysics
{
public:
	virtual ~IExcavatorPhysics() = default;

	// Puts the machine at rest in the given pose.
	virtual void Reset(const FExcavatorJointState& InitialPose) = 0;

	// Inputs for the next Advance. A threaded backend latches these.
	virtual void SetCommand(const FExcavatorCommand& Command) = 0;
	virtual void SetSoilProbe(const FBucketSoilProbe& Probe) = 0;

	// Moves the model forward by one rendered frame. Backends substep
	// internally at their own fixed step; a threaded backend may return at
	// once and publish results when ready.
	virtual void Advance(float FrameDeltaSeconds) = 0;

	// Latest published state. Safe to call from the game thread at any time.
	virtual FExcavatorPhysicsState GetState() const = 0;
};
