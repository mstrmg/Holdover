using UnrealBuildTool;

public class Holdover : ModuleRules
{
	public Holdover(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		// Headers are included relative to the module root, e.g. "Weapons/HoldoverGun.h".
		PublicIncludePaths.Add(ModuleDirectory);

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"InputCore",
			"EnhancedInput",
			"PhysicsCore",
			"Json",
			"JsonUtilities",
			"ProceduralMeshComponent", // the forest range's generated ground
			"AssetRegistry",           // finds the trees and plants placed in the forest folders
			"MeshDescription",         // the woods range bakes its ground into mesh tiles in the editor
			"StaticMeshDescription",
		});
	}
}
