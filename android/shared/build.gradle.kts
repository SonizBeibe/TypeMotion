import org.jetbrains.kotlin.gradle.dsl.JvmTarget

plugins {
    id("org.jetbrains.kotlin.multiplatform")
    id("com.android.library")
    id("org.jetbrains.compose")
    id("org.jetbrains.kotlin.plugin.compose")
}

// The engine for the iPhone (libtypemotion_ios_all.a and typemotion_ios.h),
// which the CI workflow builds with meson before this
val iosEngineDir = (findProperty("typemotion.iosEngine") as String?)?.let { file(it) }

kotlin {
    androidTarget {
        compilerOptions {
            jvmTarget.set(JvmTarget.JVM_17)
        }
    }

    iosArm64 {
        compilations.getByName("main") {
            cinterops.create("typemotion") {
                definitionFile.set(project.file("src/nativeInterop/cinterop/typemotion.def"))
                includeDirs(rootProject.file("../src/ios"))
                if (iosEngineDir != null)
                    extraOpts("-libraryPath", iosEngineDir.path)
            }
        }
        binaries.framework {
            baseName = "TypeMotionShared"
            isStatic = true
        }
    }

    sourceSets {
        commonMain.dependencies {
            implementation(compose.runtime)
            implementation(compose.foundation)
            implementation(compose.material3)
            implementation(compose.materialIconsExtended)
            implementation(compose.ui)
            implementation("org.jetbrains.kotlinx:kotlinx-coroutines-core:1.9.0")
            implementation("com.squareup.okio:okio:3.9.1")
        }
        androidMain.dependencies {
            implementation("androidx.activity:activity-compose:1.9.3")
            implementation("org.jetbrains.kotlinx:kotlinx-coroutines-android:1.9.0")
        }
    }
}

android {
    namespace = "app.typemotion.shared"
    compileSdk = 35

    defaultConfig {
        minSdk = 26
    }

    compileOptions {
        sourceCompatibility = JavaVersion.VERSION_17
        targetCompatibility = JavaVersion.VERSION_17
    }
}
