// Copyright (c) 2016 QUALCOMM Technologies Inc. All Rights Reserved.

using UnrealBuildTool;

public class LibSvrApi : ModuleRules
{
    public LibSvrApi(ReadOnlyTargetRules Target) : base(Target)
	{
		// current version of the Snapdragon SDK
		Type = ModuleType.External;

        string SnapdragonThirdPartyDirectory = UEBuildConfiguration.UEThirdPartySourceDirectory + "Qualcomm/LibSvrApi";

		if (Target.Platform == UnrealTargetPlatform.Android)
		{
            PublicLibraryPaths.Add(SnapdragonThirdPartyDirectory + "/Libs/Android/armeabi-v7a/");

            PublicAdditionalLibraries.Add(SnapdragonThirdPartyDirectory + "/Libs/Android/armeabi-v7a/libsvrapi.so");

            PublicSystemIncludePaths.Add(SnapdragonThirdPartyDirectory + "/Include");
		}
	}
}
