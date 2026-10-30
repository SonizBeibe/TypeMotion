plugins {
    id("com.android.application")
    id("org.jetbrains.kotlin.android")
    id("org.jetbrains.kotlin.plugin.compose")
}

android {
    namespace = "app.typemotion"
    compileSdk = 35

    defaultConfig {
        applicationId = "app.typemotion"
        minSdk = 26
        targetSdk = 35
        versionCode = 1
        versionName = "0.1.0"

        ndk {
            // Phones are arm64; add x86_64 for the emulator with
            // -Ptypemotion.abis=arm64-v8a,x86_64
            val abis = (findProperty("typemotion.abis") as String?) ?: "arm64-v8a"
            abiFilters += abis.split(',').map { it.trim() }
        }
    }

    buildTypes {
        release {
            isMinifyEnabled = false
            proguardFiles(getDefaultProguardFile("proguard-android-optimize.txt"), "proguard-rules.pro")
            // For sharing with friends: signed with the debug key unless a
            // release key is set up
            signingConfig = signingConfigs.getByName("debug")
        }
    }

    compileOptions {
        sourceCompatibility = JavaVersion.VERSION_17
        targetCompatibility = JavaVersion.VERSION_17
    }

    kotlinOptions {
        jvmTarget = "17"
    }

    buildFeatures {
        compose = true
    }

    sourceSets["main"].apply {
        // The official automation scripts and the translations, copied by
        // copyEngineData below
        assets.srcDir(layout.buildDirectory.dir("generated/typemotion-assets"))
        // libtypemotion.so, built from the C++ core with the NDK
        jniLibs.srcDir("../native/jniLibs")
    }
}

val copyEngineData by tasks.registering(Sync::class) {
    from("../automation") {
        into("automation")
    }
    // The modules and scripts the desktop program installs (moonscript,
    // karaskel, the aegisub.* modules...), which the scripts above use
    from("../../automation") {
        include("autoload/**", "include/**")
        exclude("**/meson.build")
        into("automation")
    }
    // The desktop program's Spanish translation, for the core's text
    from("../../po") {
        include("es.po")
        into("locale")
    }
    duplicatesStrategy = DuplicatesStrategy.EXCLUDE
    into(layout.buildDirectory.dir("generated/typemotion-assets"))
}

tasks.named("preBuild") {
    dependsOn(copyEngineData)
}

dependencies {
    val composeBom = platform("androidx.compose:compose-bom:2024.12.01")
    implementation(composeBom)
    implementation("androidx.compose.ui:ui")
    implementation("androidx.compose.foundation:foundation")
    implementation("androidx.compose.material3:material3")
    implementation("androidx.compose.material:material-icons-extended")
    implementation("androidx.activity:activity-compose:1.9.3")
    implementation("androidx.core:core-ktx:1.15.0")
    implementation("org.jetbrains.kotlinx:kotlinx-coroutines-android:1.9.0")
    debugImplementation("androidx.compose.ui:ui-tooling")
}
