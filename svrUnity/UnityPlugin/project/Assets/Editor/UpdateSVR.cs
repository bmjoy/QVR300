using UnityEngine;
using UnityEditor;
using System.IO;

public class UpdateSVR : MonoBehaviour
{
    [MenuItem("SVR/Update Libraries/All Versions")]
    static void CopyAll()
    {
        try
        {
            Debug.Log(Directory.GetCurrentDirectory());

            string libDestPath = "./Assets/SVR/Plugins/Android/";

            // 32-bit debug and release
            File.Copy("../../../svrApi/build/gradle/build/outputs/aar/svrApi-v7a-debug.aar", libDestPath + "svrApi-v7a-debug.aar", true);
            File.Copy("../../../svrApi/build/gradle/build/outputs/aar/svrApi-v7a-release.aar", libDestPath + "svrApi-v7a-release.aar", true);
            File.Copy("../build/gradle/build/outputs/aar/svrUnity-v7a-debug.aar", libDestPath + "svrUnity-v7a-debug.aar", true);
            File.Copy("../build/gradle/build/outputs/aar/svrUnity-v7a-release.aar", libDestPath + "svrUnity-v7a-release.aar", true);
            // 64-bit + 32-bit debug and release
            File.Copy("../../../svrApi/build/gradle/build/outputs/aar/svrApi-debug.aar", libDestPath + "svrApi-debug.aar", true);
            File.Copy("../../../svrApi/build/gradle/build/outputs/aar/svrApi-release.aar", libDestPath + "svrApi-release.aar", true);
            File.Copy("../build/gradle/build/outputs/aar/svrUnity-debug.aar", libDestPath + "svrUnity-debug.aar", true);
            File.Copy("../build/gradle/build/outputs/aar/svrUnity-release.aar", libDestPath + "svrUnity-release.aar", true);

            Debug.Log("Updated SVR Libraries");
        }
        catch (IOException e)
        {
            Debug.LogError(e.Message);
        }
    }

    [MenuItem("SVR/Update Libraries/32-bit/Debug")]
    static void Copy32bitDebug()
    {
        try
        {
            Debug.Log(Directory.GetCurrentDirectory());

            string libDestPath = "./Assets/SVR/Plugins/Android/";

            // 32-bit only
            File.Copy("../../../svrApi/build/gradle/build/outputs/aar/svrApi-v7a-debug.aar", libDestPath + "svrApi-v7a-debug.aar", true);
            File.Copy("../build/gradle/build/outputs/aar/svrUnity-v7a-debug.aar", libDestPath + "svrUnity-v7a-debug.aar", true);
            // 64-bit + 32-bit
            //File.Copy("../../../svrApi/build/gradle/build/outputs/aar/svrApi-debug.aar", libDestPath + "svrApi-debug.aar", true);
            //File.Copy("../build/gradle/build/outputs/aar/svrUnity-debug.aar", libDestPath + "svrUnity-debug.aar", true);

            Debug.Log("Updated SVR Libraries");
        }
        catch (IOException e)
        {
            Debug.LogError(e.Message);
        }
    }
    [MenuItem("SVR/Update Libraries/32-bit/Release")]
    static void Copy32bitRelease()
    {
        try
        {
            Debug.Log(Directory.GetCurrentDirectory());

            string libDestPath = "./Assets/SVR/Plugins/Android/";

            // 32-bit only
            File.Copy("../../../svrApi/build/gradle/build/outputs/aar/svrApi-v7a-release.aar", libDestPath + "svrApi-v7a-release.aar", true);
            File.Copy("../build/gradle/build/outputs/aar/svrUnity-v7a-release.aar", libDestPath + "svrUnity-v7a-release.aar", true);
            // 64-bit + 32-bit
            //File.Copy("../../../svrApi/build/gradle/build/outputs/aar/svrApi-debug.aar", libDestPath + "svrApi-debug.aar", true);
            //File.Copy("../build/gradle/build/outputs/aar/svrUnity-debug.aar", libDestPath + "svrUnity-debug.aar", true);

            Debug.Log("Updated SVR Libraries");
        }
        catch (IOException e)
        {
            Debug.LogError(e.Message);
        }
    }
    [MenuItem("SVR/Update Libraries/64-bit/Debug")]
    static void Copy64bitDebug()
    {
        try
        {
            Debug.Log(Directory.GetCurrentDirectory());

            string libDestPath = "./Assets/SVR/Plugins/Android/";

            // 32-bit only
            //File.Copy("../../../svrApi/build/gradle/build/outputs/aar/svrApi-v7a-debug.aar", libDestPath + "svrApi-v7a-debug.aar", true);
            //File.Copy("../build/gradle/build/outputs/aar/svrUnity-v7a-debug.aar", libDestPath + "svrUnity-v7a-debug.aar", true);
            // 64-bit + 32-bit
            File.Copy("../../../svrApi/build/gradle/build/outputs/aar/svrApi-debug.aar", libDestPath + "svrApi-debug.aar", true);
            File.Copy("../build/gradle/build/outputs/aar/svrUnity-debug.aar", libDestPath + "svrUnity-debug.aar", true);

            Debug.Log("Updated SVR Libraries");
        }
        catch (IOException e)
        {
            Debug.LogError(e.Message);
        }
    }
    [MenuItem("SVR/Update Libraries/64-bit/Release")]
    static void Copy64bitRelease()
    {
        try
        {
            Debug.Log(Directory.GetCurrentDirectory());

            string libDestPath = "./Assets/SVR/Plugins/Android/";

            // 32-bit only
            //File.Copy("../../../svrApi/build/gradle/build/outputs/aar/svrApi-v7a-debug.aar", libDestPath + "svrApi-v7a-debug.aar", true);
            //File.Copy("../build/gradle/build/outputs/aar/svrUnity-v7a-debug.aar", libDestPath + "svrUnity-v7a-debug.aar", true);
            // 64-bit + 32-bit
            File.Copy("../../../svrApi/build/gradle/build/outputs/aar/svrApi-release.aar", libDestPath + "svrApi-release.aar", true);
            File.Copy("../build/gradle/build/outputs/aar/svrUnity-release.aar", libDestPath + "svrUnity-release.aar", true);

            Debug.Log("Updated SVR Libraries");
        }
        catch (IOException e)
        {
            Debug.LogError(e.Message);
        }
    }

}
