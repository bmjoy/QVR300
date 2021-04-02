// Copyright (c) 2016 QUALCOMM Technologies Inc. All Rights Reserved.

using System.IO;

namespace UnrealBuildTool.Rules
{
	public class SnapdragonVRController : ModuleRules
	{
        public SnapdragonVRController(ReadOnlyTargetRules Target) : base(Target)
		{
			PrivateIncludePaths.AddRange(
				new string[] {
					"SnapdragonVRController/Private",
                    "../../../../../Source/Runtime/Core/Private"
					// ... add other private include paths required here ...
				}
				);

			PrivateDependencyModuleNames.AddRange(
				new string[]
				{
					"Core",
					"CoreUObject",
					"Engine",
					"InputCore",
					"InputDevice",
					"HeadMountedDisplay",
					"UMG",
					"Slate",
					"SlateCore"
				}
				);

            PrivateIncludePaths.AddRange(
				new string[] {
					// ... add other private include paths required here ...
					}
				);
			if (Target.Platform == UnrealTargetPlatform.Android)
			{
				PrivateDependencyModuleNames.AddRange(new string[] { "LibSvrApi" });

				string PluginPath = Utils.MakePathRelativeTo(ModuleDirectory, BuildConfiguration.RelativeEnginePath);
                AdditionalPropertiesForReceipt.Add(new ReceiptProperty("AndroidPlugin", Path.Combine(PluginPath, "SnapdragonVRController_APL.xml")));
			}

		}
	}
}
