#include "KinematicExcavatorPhysics.h"

namespace
{
	// A long hitch (breakpoint, level load) should not replay seconds of motion.
	constexpr float MaxFrameSeconds = 0.1f;
}

void FKinematicExcavatorPhysics::Reset(const FExcavatorJointState& InitialPose)
{
	State = FExcavatorPhysicsState();
	State.Joints = InitialPose;
	State.Joints.SwingRate = State.Joints.BoomRate = State.Joints.ArmRate = State.Joints.BucketRate = 0.f;
	Command = FExcavatorCommand();
	LeftTrack = RightTrack = 0.f;
	Accumulator = 0.f;
}

void FKinematicExcavatorPhysics::Advance(float FrameDeltaSeconds)
{
	const float StepSeconds = FMath::Max(Params.StepSeconds, 1e-4f);
	Accumulator += FMath::Clamp(FrameDeltaSeconds, 0.f, MaxFrameSeconds);
	while (Accumulator >= StepSeconds)
	{
		Step(StepSeconds);
		Accumulator -= StepSeconds;
	}
}

void FKinematicExcavatorPhysics::Step(float Dt)
{
	// Rates ease toward the command, which reads like hydraulic lag.
	const float Response = Params.HydraulicResponse;
	auto Drive = [Dt, Response](float& Angle, float& Rate, float Cmd, float Speed, const FVector2D* Limits)
	{
		Rate = FMath::FInterpTo(Rate, Cmd * Speed, Dt, Response);
		Angle += Rate * Dt;
		if (Limits && (Angle < Limits->X || Angle > Limits->Y))
		{
			Angle = FMath::Clamp(Angle, static_cast<float>(Limits->X), static_cast<float>(Limits->Y));
			Rate = 0.f;
		}
	};

	FExcavatorJointState& J = State.Joints;
	Drive(J.SwingDeg, J.SwingRate, Command.Swing, Params.SwingSpeed, nullptr);
	Drive(J.BoomDeg, J.BoomRate, Command.Boom, Params.BoomSpeed, &Params.BoomLimits);
	Drive(J.ArmDeg, J.ArmRate, Command.Arm, Params.ArmSpeed, &Params.ArmLimits);
	Drive(J.BucketDeg, J.BucketRate, Command.Bucket, Params.BucketSpeed, &Params.BucketLimits);
	J.SwingDeg = FRotator::NormalizeAxis(J.SwingDeg);

	LeftTrack = FMath::FInterpTo(LeftTrack, Command.LeftTrack, Dt, Response * 0.5f);
	RightTrack = FMath::FInterpTo(RightTrack, Command.RightTrack, Dt, Response * 0.5f);
	State.ForwardSpeed = (LeftTrack + RightTrack) * 0.5f * Params.TravelSpeed;
	State.YawRate = FMath::RadiansToDegrees((LeftTrack - RightTrack) * Params.TravelSpeed / FMath::Max(Params.TrackGauge, 1.f));

	State.BucketResistance = FVector::ZeroVector;
}
