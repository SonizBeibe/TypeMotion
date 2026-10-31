# TypeMotion para Android

La app de teléfono de TypeMotion. Usa el mismo núcleo en C++ que el
programa de escritorio (`src/`, compilado sin wxWidgets) a través de
`libtypemotion.so`, y una interfaz en Kotlin con Compose.

Las pantallas y el estado de la app están en el módulo `shared`
(Compose Multiplatform), que también usa la app de iPhone (ver
`../ios/README.md`); el módulo `app` es solo lo de Android. En Windows,
Gradle se salta las partes de iPhone de `shared`.

## Compilar

1. Compila el núcleo para arm64 con el NDK (27.2; ajusta su carpeta en
   `cross/android-arm64.ini`) y deja la biblioteca, sin símbolos, en
   `native/jniLibs/arm64-v8a/libtypemotion.so`. Desde la raíz del
   repositorio, en una consola con el entorno de MSVC cargado (meson
   compila con él las herramientas de LuaJIT y fribidi) y con
   `C:\Program Files\Git\usr\bin` al final del `PATH` (`sh` y `sed`):

   ```
   meson setup ..\build-android\arm64-video --cross-file android\cross\android-arm64.ini -Dcore_only=true -Dtests=false -Dffms2=enabled --force-fallback-for=ffms2 -Dlocal_boost=true -Ddefault_library=static -Dbuildtype=release -Db_pch=false -Dlibass:require-system-font-provider=false -Dlibass:checkasm=disabled -Dlibass:asm=disabled -Dffmpeg:programs=disabled -Dffmpeg:ffmpeg=disabled -Dffmpeg:ffplay=disabled -Dffmpeg:ffprobe=disabled -Dffmpeg:tests=disabled -Dffmpeg:avdevice=disabled -Dffmpeg:avfilter=disabled -Dffmpeg:postproc=disabled -Dffmpeg:encoders=disabled -Dffmpeg:muxers=disabled -Dffmpeg:filters=disabled -Dffmpeg:hwaccels=disabled -Dffmpeg:devices=disabled -Dffmpeg:indevs=disabled -Dffmpeg:outdevs=disabled -Dffmpeg:network=disabled -Dffmpeg:protocols=disabled -Dffmpeg:file_protocol=enabled -Dffmpeg:jni=disabled -Dffmpeg:mediacodec=disabled -Dffmpeg:android=disabled -Dffmpeg:bzlib=disabled -Dffmpeg:lzma=disabled -Dffmpeg:libdav1d=enabled -Dffmpeg:zlib=enabled -Ddav1d:enable_tools=false -Ddav1d:enable_tests=false -Ddav1d:enable_examples=false -Ddav1d:enable_docs=false
   ninja -C ..\build-android\arm64-video libtypemotion.so
   llvm-strip --strip-unneeded -o android\native\jniLibs\arm64-v8a\libtypemotion.so ..\build-android\arm64-video\libtypemotion.so
   ```

   El video y el audio se abren con FFmpeg (solo lo que hace falta para
   decodificar archivos locales), dav1d y ffms2, como en el escritorio;
   la salida de audio usa AAudio.

   Para el emulador, lo mismo con `android-x86_64.ini` y la carpeta
   `x86_64-video`, con `subprojects\nasm-3.01` en el `PATH` (FFmpeg lo
   necesita para x86_64), y compila el APK con
   `-Ptypemotion.abis=arm64-v8a,x86_64`. Meson aplica solo el arreglo de
   fribidi para compilar en cruzado
   (`subprojects/packagefiles/fribidi-native-cc.diff`) al descargarlo.
2. Compila el APK:

   ```
   ./gradlew assembleRelease
   ```

   El APK queda en `app/build/outputs/apk/release/`. Va firmado con la
   clave de depuración, para instalarlo directo en tu teléfono y en los
   de tus amigos.

## Carpetas en el teléfono

- `TypeMotion/automation/autoload`: tus scripts `.lua` y `.moon`, que se
  cargan siempre, como la carpeta autoload del escritorio.
- `TypeMotion/automation/include`: los módulos que tus scripts incluyen.
- `TypeMotion/fonts`: fuentes extra, que ganan a las del teléfono.

La app pide el permiso de acceso a todos los archivos para abrir y
guardar subtítulos por su ruta y para usar esa carpeta. Los scripts
oficiales (`automation/` de este proyecto) vienen dentro del APK.
