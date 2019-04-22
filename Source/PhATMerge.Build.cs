
namespace UnrealBuildTool.Rules
{
    using System.IO;

	public class PhATMerge : ModuleRules
	{
		public PhATMerge(ReadOnlyTargetRules Target) : base(Target)
        {
            bEnforceIWYU = true;

            PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

            MinFilesUsingPrecompiledHeaderOverride = 1;
            bFasterWithoutUnity = true;

            PublicDependencyModuleNames.AddRange(
                new string[]
				{
					"Core",
					"CoreUObject",
                    "Engine",
                    "InputCore",
				}
            );
        }
    }
}