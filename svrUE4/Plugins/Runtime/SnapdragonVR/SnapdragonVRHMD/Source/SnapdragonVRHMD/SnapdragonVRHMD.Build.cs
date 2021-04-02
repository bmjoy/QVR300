// Copyright (c) 2016 QUALCOMM Technologies Inc. All Rights Reserved.

using System.IO;

namespace UnrealBuildTool.Rules
{
	public class SnapdragonVRHMD : ModuleRules
	{
        public SnapdragonVRHMD(ReadOnlyTargetRules Target) : base(Target)
		{
			PrivateIncludePaths.AddRange(
				new string[] {
					"SnapdragonVRHMD/Private",
                    "../../../../../Source/Runtime/Renderer/Private",
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
					"RHI",
					"RenderCore",
					"Renderer",
					"ShaderCore",
					"Launch",
					"HeadMountedDisplay",
                    "Slate",
                    "SlateCore",
				}
				);

			PrivateDependencyModuleNames.AddRange(new string[] { "OpenGLDrv" });
			AddEngineThirdPartyPrivateStaticDependencies(Target, "OpenGL");
            PrivateIncludePaths.AddRange(
				new string[] {
					"../../../../../Source/Runtime/OpenGLDrv/Private",
					// ... add other private include paths required here ...
					}
				);
			if (Target.Platform == UnrealTargetPlatform.Android)
			{
				PrivateDependencyModuleNames.AddRange(new string[] { "LibSvrApi" });

				string PluginPath = Utils.MakePathRelativeTo(ModuleDirectory, BuildConfiguration.RelativeEnginePath);
                AdditionalPropertiesForReceipt.Add(new ReceiptProperty("AndroidPlugin", Path.Combine(PluginPath, "SnapdragonVRHMD_APL.xml")));
			}

		}
	}
}
