//
// Copyright (c) 2008-2022 the Urho3D project.
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.
//

plugins {
    id("com.android.application")
    kotlin("android")
    kotlin("android.extensions")
}

val kotlinVersion: String by ext
val ndkSideBySideVersion: String by ext
val cmakeVersion: String by ext
val buildStagingDir: String by ext

android {
    ndkVersion = ndkSideBySideVersion
    compileSdkVersion(29)
    buildToolsVersion = "30.0.2"
    defaultConfig {
        minSdkVersion(18)
        targetSdkVersion(29)
        applicationId = "io.urho3d.launcher"
        versionCode = 1
        versionName = project.version.toString()
        testInstrumentationRunner = "android.support.test.runner.AndroidJUnitRunner"
        externalNativeBuild {
            cmake {
				arguments.apply {                
		                System.getenv("ANDROID_CCACHE")?.let { add("-D ANDROID_CCACHE=$it") }
		                // Pass along matching env-vars as CMake build options 	                
		                arguments += listOf("-D URHO3D_CLANG_TOOLS=0", "-D URHO3D_LUA=0", "-D URHO3D_LUAJIT=0", "-D URHO3D_LUAJIT_AMALG=0")  
		                arguments += listOf("-D URHO3D_SAFE_LUA=0", "-D DEFAULT_LUA_RAW=0", "-D URHO3D_PCH=1", "-D URHO3D_DATABASE_SQLITE=1", "-D URHO3D_FILEWATCHER=0")
		                arguments += listOf("-D URHO3D_TOOLS=0", "-D URHO3D_DOCS=0", "-D URHO3D_DOCS_QUIET=0")
				}             
            }
        }
    }
    flavorDimensions += "graphics"
    productFlavors {
    	create("samples") {
            dimension = "graphics"
            versionNameSuffix = ""
		    externalNativeBuild {
		        cmake {
        			arguments += listOf("-D URHO3D_LIB_TYPE=SHARED", "-D URHO3D_C++11=1")
        			arguments += listOf("-D URHO3D_OPENGL=1", "-D URHO3D_VULKAN=0", "-D URHO3D_VOLK=0", "-D URHO3D_VMA=0")
                    arguments += listOf("-D URHO3D_ANGELSCRIPT=1", "-D URHO3D_PLAYER=1", "-D URHO3D_SAMPLES=1")   
	                arguments += listOf("-D URHO3D_IK=1", "-D URHO3D_NAVIGATION=0", "-D URHO3D_PHYSICS=0", "-D URHO3D_WEBP=1")                                     
		        }
		    }
			splits {
				abi {
					//isEnable = project.hasProperty("ANDROID_ABI")
					isEnable = true
					reset()
                	include("arm64-v8a")
				}
			}
    	}
    }    
    buildTypes {  
        named("debug") {      
            isMinifyEnabled = false        
        }       
        named("release") {
            isMinifyEnabled = false
            proguardFiles(getDefaultProguardFile("proguard-android-optimize.txt"), "proguard-rules.pro")
        }  
    }
    lintOptions {
        isAbortOnError = false
    }
    compileOptions {
        sourceCompatibility = JavaVersion.VERSION_1_8
        targetCompatibility = JavaVersion.VERSION_1_8
    }
    kotlinOptions {
        jvmTarget = "1.8"
    }  
    externalNativeBuild {
        cmake {
            version = cmakeVersion
            path = project.file("CMakeLists.txt")
            buildStagingDirectory(buildStagingDir)
        }
    }
}

dependencies {
//    implementation("io.urho3d:urho3d-lib-samplesDebug:Unversioned")
    implementation(project(":android:urho3d-lib"))
    implementation(fileTree(mapOf("dir" to "libs", "include" to listOf("*.jar", "*.aar"))))
    implementation("org.jetbrains.kotlin:kotlin-stdlib-jdk8:$kotlinVersion")
    implementation("androidx.core:core-ktx:1.3.2")
    implementation("androidx.appcompat:appcompat:1.2.0")
    implementation("androidx.constraintlayout:constraintlayout:2.0.2")
    testImplementation("junit:junit:4.13.1")
    androidTestImplementation("androidx.test:runner:1.3.0")
    androidTestImplementation("androidx.test.espresso:espresso-core:3.3.0")
}

//afterEvaluate {
//    android.buildTypes.forEach {
//    
//        val config = it.name.capitalize()
//        tasks {
//            "externalNativeBuild$config" {
//                mustRunAfter(":android:urho3d-lib:externalNativeBuild$config")
//            }
//        }
//   }
//}

tasks {
    register<Delete>("cleanAll") {
        dependsOn("clean")
        delete = setOf(android.externalNativeBuild.cmake.buildStagingDirectory)
    }
}
