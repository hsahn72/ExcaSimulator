#pragma once

#include "CoreMinimal.h"
#include "Materials/MaterialInstanceDynamic.h"

// Placeholder look for the prototype: engine basic shapes tinted through
// BasicShapeMaterial's "Color" parameter. Real models replace these later.
namespace ExcaAssets
{
	inline const TCHAR* const CubePath = TEXT("/Engine/BasicShapes/Cube.Cube");
	inline const TCHAR* const SpherePath = TEXT("/Engine/BasicShapes/Sphere.Sphere");
	inline const TCHAR* const BaseMaterialPath = TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial");

	inline const FLinearColor SoilColor(0.20f, 0.12f, 0.06f);

	inline UMaterialInstanceDynamic* MakeColor(UMaterialInterface* Base, UObject* Outer, const FLinearColor& Color)
	{
		if (!Base)
		{
			return nullptr;
		}
		UMaterialInstanceDynamic* Mid = UMaterialInstanceDynamic::Create(Base, Outer);
		Mid->SetVectorParameterValue(TEXT("Color"), Color);
		return Mid;
	}
}
