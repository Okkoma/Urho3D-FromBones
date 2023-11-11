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

import org.gradle.internal.io.NullOutputStream
import org.gradle.internal.os.OperatingSystem

plugins {
    id("com.android.library")
 //   id("com.jfrog.bintray")
    kotlin("android")
    kotlin("android.extensions")
    `maven-publish`
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
        versionCode = 1
        versionName = project.version.toString()
        testInstrumentationRunner = "android.support.test.runner.AndroidJUnitRunner"
		externalNativeBuild {
			cmake {
				arguments.apply {                
		                System.getenv("ANDROID_CCACHE")?.let { add("-D ANDROID_CCACHE=$it") }
		                // Pass along matching env-vars as CMake build options
		                add("-DAS_IS_BUILDING")    	                
		                arguments += listOf("-D URHO3D_CLANG_TOOLS=0", "-D URHO3D_LUA=0", "-D URHO3D_LUAJIT=0", "-D URHO3D_LUAJIT_AMALG=0")  
		                arguments += listOf("-D URHO3D_SAFE_LUA=0", "-D DEFAULT_LUA_RAW=0", "-D URHO3D_PCH=1", "-D URHO3D_DATABASE_SQLITE=1", "-D URHO3D_FILEWATCHER=0")
		                arguments += listOf("-D URHO3D_TOOLS=0", "-D URHO3D_DOCS=0", "-D URHO3D_DOCS_QUIET=0")
				}
		        targets.add("Urho3D")
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
        create("gl") {
            dimension = "graphics"
            versionNameSuffix = "-gl"
		    externalNativeBuild {
		        cmake {
			        arguments += listOf("-D URHO3D_LIB_TYPE=STATIC", "-D URHO3D_C++11=0")
        			arguments += listOf("-D URHO3D_OPENGL=1", "-D URHO3D_VULKAN=0", "-D URHO3D_VOLK=0", "-D URHO3D_VMA=0")	
	                arguments += listOf("-D URHO3D_ANGELSCRIPT=0", "-D URHO3D_PLAYER=0", "-D URHO3D_SAMPLES=0")   
	                arguments += listOf("-D URHO3D_IK=0", "-D URHO3D_NAVIGATION=0", "-D URHO3D_PHYSICS=0", "-D URHO3D_WEBP=0")        			                                  
		        }
		    }
			splits {
				abi {
					isEnable = project.hasProperty("ANDROID_ABI")
					reset()
					include(
						*(project.findProperty("ANDROID_ABI") as String? ?: "")
						.split(',')
						.toTypedArray()
					)          
				}
			}      		                
        }
        create("vk") {
            dimension = "graphics"
            versionNameSuffix = "-vk"
		    externalNativeBuild {
		        cmake {
			        arguments += listOf("-D URHO3D_LIB_TYPE=STATIC", "-D URHO3D_C++11=1")
					arguments += listOf("-D URHO3D_OPENGL=0", "-D URHO3D_VULKAN=1", "-D URHO3D_VOLK=1", "-D URHO3D_VMA=1")
		            arguments += listOf("-D URHO3D_ANGELSCRIPT=0", "-D URHO3D_PLAYER=0", "-D URHO3D_SAMPLES=0")   
		            arguments += listOf("-D URHO3D_IK=0", "-D URHO3D_NAVIGATION=0", "-D URHO3D_PHYSICS=0", "-D URHO3D_WEBP=0")					
		        }
		    }     
			splits {
				abi {
					isEnable = project.hasProperty("ANDROID_ABI")
					reset()
					include(
						*(project.findProperty("ANDROID_ABI") as String? ?: "")
						.split(',')
						.toTypedArray()
					)          
				}
			}        
        }
    }
    buildTypes {
        named("debug") {      
            isMinifyEnabled = false
		    isJniDebuggable = true
		    /*
		    externalNativeBuild {
		        cmake {
					arguments += listOf("-D URHO3D_VULKAN_VALIDATION=1")
		        }
		    }    
		    */          
        }    
        named("release") {
            isMinifyEnabled = false
            //proguardFiles(getDefaultProguardFile("proguard-android-optimize.txt"), "proguard-rules.pro")
        }
    }
    externalNativeBuild {
        cmake {
            version = cmakeVersion
            path = project.file("../../CMakeLists.txt")
            buildStagingDirectory(buildStagingDir)
        }
    }
    sourceSets {
        named("main") {
            java.srcDir("../../Source/ThirdParty/SDL/android-project/app/src/main/java")
        }
    }
}

dependencies {
    implementation(fileTree(mapOf("dir" to "libs", "include" to listOf("*.jar", "*.aar"))))
    implementation("org.jetbrains.kotlin:kotlin-stdlib-jdk8:$kotlinVersion")
    implementation("com.getkeepsafe.relinker:relinker:1.4.2")
    testImplementation("junit:junit:4.13.1")
    implementation("androidx.core:core-ktx:1.3.2")
    implementation("androidx.appcompat:appcompat:1.2.0")
    implementation("androidx.constraintlayout:constraintlayout:2.0.2")    
    androidTestImplementation("androidx.test:runner:1.3.0")
    androidTestImplementation("androidx.test.espresso:espresso-core:3.3.0")
}

android.libraryVariants.whenObjectAdded {
    val config = name
    packageLibraryProvider.get().apply {
        // Customize bundle task to also zip the Urho3D headers and libraries
        File(android.externalNativeBuild.cmake.buildStagingDirectory, "cmake/$config").list()?.forEach { abi ->
            listOf("include", "lib").forEach {
                from(File(android.externalNativeBuild.cmake.buildStagingDirectory, "cmake/$config/$abi/$it")) {
                    into("urho3d/$config/$abi/$it")
                }
            }
        }
    }
}

tasks {
    register<Delete>("cleanAll") {
        dependsOn("clean")
        delete = setOf(android.externalNativeBuild.cmake.buildStagingDirectory)
    }
    register<Jar>("sourcesJar") {
        archiveClassifier.set("sources")
        from(android.sourceSets.getByName("main").java.srcDirs)
    }
/*
    register<Zip>("documentationZip") {
        archiveClassifier.set("documentation")
        dependsOn("makeDoc")
    }
    register<Exec>("makeDoc") {
        // Ignore the exit status on Windows host system because Doxygen may not return exit status correctly on Windows
        isIgnoreExitValue = OperatingSystem.current().isWindows
        standardOutput = NullOutputStream.INSTANCE
        args("--build", ".", "--target", "doc")
        dependsOn("makeDocConfigurer")
    }
    register<Task>("makeDocConfigurer") {
        dependsOn("generateJsonModelRelease")
        doLast {
            val abi = File(android.externalNativeBuild.cmake.buildStagingDirectory, "cmake/release").list()!!.first()
            val buildTree = File(android.externalNativeBuild.cmake.buildStagingDirectory, "cmake/release/$abi")
            named<Exec>("makeDoc") {
                // This is a hack - expect the first line to contain the path to the CMake executable
                executable = File(buildTree, "build_command.txt").readLines().first().split(":").last().trim()
                workingDir = buildTree
            }
            named<Zip>("documentationZip") {
                from(File(buildTree, "Docs/html")) {
                    into("docs")
                }
            }
        }
    }
*/
}

publishing {

    publications {

        androidComponents {
            onVariants { variantBuilder ->
                register<MavenPublication>("Urho${variantBuilder.name.capitalize()}") {
                    configure("${variantBuilder.flavorName}${variantBuilder.buildType?.capitalize()}")
                }
            }
        }

    }
/*    repositories {
        maven {
            name = "GitHubPackages"
           url = uri("https://maven.pkg.github.com/urho3d/Urho3D")
            credentials {
                username = System.getenv("GITHUB_ACTOR")
                password = System.getenv("GITHUB_TOKEN")
            }
        }
    }
*/
}

/*
bintray {
    user = System.getenv("BINTRAY_USER")
    key = System.getenv("BINTRAY_KEY")
    publish = true
    override = true
    setPublications("UrhoRelease", "UrhoDebug")
    pkg.apply {
        repo = "maven"
        name = project.name
        setLicenses("MIT")
        vcsUrl = "https://github.com/urho3d/Urho3D.git"
        userOrg = "urho3d"
        setLabels("android", "game-development", "game-engine", "open-source", "urho3d")
        websiteUrl = "https://urho3d.io/"
        issueTrackerUrl = "https://github.com/urho3d/Urho3D/issues"
        githubRepo = "urho3d/Urho3D"
        publicDownloadNumbers = true
        desc = project.description
        version.apply {
            name = project.version.toString()
            desc = project.description
        }
    }
}
*/

fun MavenPublication.configure(config: String) {
    groupId = project.group.toString()
    artifactId = "${project.name}-${config}"
    afterEvaluate {
        from(components[config])
    }
    artifact(tasks["sourcesJar"])
    //artifact(tasks["documentationZip"])
    pom {
        inceptionYear.set("2008")
        licenses {
            license {
                name.set("MIT License")
                url.set("https://github.com/urho3d/Urho3D/blob/master/LICENSE")
            }
        }
        developers {
            developer {
                name.set("Urho3D contributors")
                url.set("https://github.com/urho3d/Urho3D/graphs/contributors")
            }
        }
        //scm {
        //    url.set("https://github.com/urho3d/Urho3D.git")
        //    connection.set("scm:git:ssh://git@github.com:urho3d/Urho3D.git")
        //    developerConnection.set("scm:git:ssh://git@github.com:urho3d/Urho3D.git")
        //}
        withXml {
            asNode().apply {
                appendNode("name", "Urho3D")
                appendNode("description", project.description)
                appendNode("url", "https://urho3d.io/")
            }
        }
    }
}
