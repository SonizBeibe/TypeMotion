# TypeMotion para iPhone

La app de iPhone usa el mismo núcleo en C++ que la de Android
(`src/android`, más `src/ios/capi.cpp`) y las mismas pantallas en Kotlin
(`android/shared`, con Compose Multiplatform). Solo se compila en un Mac, así
que lo hace GitHub Actions: el flujo `.github/workflows/ios.yml` deja un
`TypeMotion.ipa` sin firmar entre los artefactos de cada ejecución.

## Cómo se compila

1. **Núcleo**: meson con `ios/cross/ios-arm64.ini` compila `typemotion_ios`
   (LuaJIT sin JIT, porque iOS no deja generar código máquina) y el flujo
   une esa biblioteca con todas las que usa en `libtypemotion_ios_all.a`.
2. **Pantallas**: Gradle compila `android/shared` para iOS
   (`:shared:linkReleaseFrameworkIosArm64`) en `TypeMotionShared.framework`,
   que llama al núcleo por `src/ios/typemotion_ios.h`.
3. **App**: XcodeGen genera el proyecto desde `ios/app/project.yml`, con los
   scripts oficiales y la traducción, y `xcodebuild` lo compila sin firmar.
   El `.app` se empaqueta en `Payload/` dentro de `TypeMotion.ipa`.

## Instalarla desde Windows

1. Descarga `TypeMotion.ipa` de la última ejecución de "iOS app" en la
   pestaña Actions del repositorio (viene dentro de un .zip).
2. Instala iTunes e iCloud desde la web de Apple (no los de Microsoft Store)
   y [Sideloadly](https://sideloadly.io).
3. Conecta el iPhone por cable, arrastra el `.ipa` a Sideloadly, escribe tu
   Apple ID y pulsa Start.
4. En el iPhone: Ajustes › Privacidad y seguridad › Modo de desarrollador
   (actívalo y reinicia), y Ajustes › General › VPN y gestión de
   dispositivos para confiar en tu Apple ID.

Con un Apple ID gratis la firma dura 7 días; para renovarla, vuelve a
instalarla igual con Sideloadly (tus archivos se quedan).

## Carpetas en el iPhone

En la app Archivos, "En mi iPhone › TypeMotion" es la carpeta de la app:

- Pon ahí los subtítulos y videos que quieras abrir.
- `automation/autoload`: tus scripts `.lua` y `.moon`.
- `automation/include`: los módulos que tus scripts incluyen.
- `fonts`: fuentes extra.
