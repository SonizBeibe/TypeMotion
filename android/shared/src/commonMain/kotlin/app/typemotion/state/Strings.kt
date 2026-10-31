package app.typemotion.state

/** Texts of the app itself; the engine's messages come from the core */
object Strings {
    const val notAvailable = "Esta ventana todavía no está disponible en el teléfono"
    const val nothingToSave = "No hay cambios sin guardar"
    const val openSubtitles = "Abrir subtítulos"
    const val subtitleWildcard =
        "Todos los formatos (*.ass, *.ssa, *.srt, *.sub, *.txt, *.ttxt)|*.ass;*.ssa;*.srt;*.sub;*.txt;*.ttxt|" +
            "Advanced SubStation Alpha (*.ass)|*.ass|SubRip (*.srt)|*.srt|Todos los archivos (*.*)|*.*"
    const val scriptsReloaded = "Scripts recargados"
    const val openVideo = "Abrir video"
    const val openAudio = "Abrir audio"
    const val videoWildcard =
        "Formatos de video (*.mkv, *.mp4, *.avi, *.webm, *.mov, ...)|" +
            "*.asf;*.avi;*.avs;*.d2v;*.h264;*.hevc;*.m2ts;*.m4v;*.mkv;*.mov;*.mp4;*.mpeg;*.mpg;*.ogm;*.webm;*.wmv;*.ts;*.y4m;*.yuv|" +
            "Todos los archivos (*.*)|*.*"
    const val audioWildcard =
        "Formatos de audio (*.aac, *.ac3, *.flac, *.m4a, *.mka, *.mp3, *.ogg, *.opus, *.wav, ...)|" +
            "*.aac;*.ac3;*.ape;*.dts;*.eac3;*.flac;*.m4a;*.mka;*.mp3;*.mp4;*.ogg;*.opus;*.w64;*.wav;*.wma|" +
            "Formatos de video (*.mkv, *.mp4, *.avi, *.webm, ...)|*.avi;*.mkv;*.mov;*.mp4;*.ogm;*.webm;*.wmv;*.ts|" +
            "Todos los archivos (*.*)|*.*"

    fun scriptImported(name: String) = "$name copiado a tus automatizaciones"
}
