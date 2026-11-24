#pragma once

#include "ExcavatorPhysics.h"

// Tuning for the kinematic model. AExcavatorPawn copies its Details-panel
// values in here every frame so they can be changed while playing.
struct FKinematicExcavatorParams
{
	// Joint speeds at full stick, deg/s.
	float SwingSpeed = 30.f;
	float BoomSpeed = 22.f;
	float ArmSpeed = 30.f;
	float BucketSpeed = 50.f;

	// Track speed at full stick, cm/s.
	float TravelSpeed = 150.f;
	float TrackGauge = 220.f;

	// How quickly rates reach the command; lower feels heavier.
	float HydraulicResponse = 5.f;

	// Joint limits in degrees (min, max), relative to the parent link.
	FVector2D BoomLimits = FVector2D(-25.f, 60.f);
	FVector2D ArmLimits = FVector2D(-155.f, -35.f);
	FVector2D BucketLimits = FVector2D(-160.f, 30.f);

	// Internal step, seconds.
	float StepSeconds = 1.f / 240.f;
};

// The stage 1 model: each joint eases toward the commanded speed and stops at
// its limits. No masses, no hydraulics, no soil resistance.
class FKinematicExcavatorPhysics final : public IExcavatorPhysics
{
public:
	FKinematicExcavatorParams Params;

	virtual void Reset(const FExcavatorJointState& InitialPose) override;
	virtual void SetCommand(const FExcavatorCommand& InCommand) override { Command = InCommand; }
	virtual void SetSoilProbe(const FBucketSoilProbe& InProbe) override { Probe = InProbe; }
	virtual void Advance(float FrameDeltaSeconds) override;
	virtual FExcavatorPhysicsState GetState() const override { return State; }

private:
	void Step(float Dt);

	FExcavatorCommand Command;
	FBucketSoilProbe Probe;
	FExcavatorPhysicsState State;
	float LeftTrack = 0.f;
	float RightTrack = 0.f;
	float Accumulator = 0.f;
};
