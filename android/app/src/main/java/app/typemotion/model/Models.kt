package app.typemotion.model

import app.typemotion.engine.bool
import app.typemotion.engine.double
import app.typemotion.engine.int
import app.typemotion.engine.objects
import app.typemotion.engine.str
import app.typemotion.engine.strings
import org.json.JSONObject

/** The commit types of AssFile, which "commit" events carry */
object Commit {
    const val NEW = 0
    const val ORDER = 0x1
    const val SCRIPTINFO = 0x2
    const val STYLES = 0x4
    const val ATTACHMENT = 0x8
    const val DIAG_ADDREM = 0x10
    const val DIAG_META = 0x20
    const val DIAG_TIME = 0x40
    const val DIAG_TEXT = 0x80
    const val DIAG_FULL = DIAG_META or DIAG_TIME or DIAG_TEXT
    const val EXTRADATA = 0x100
}

/** A subtitle line, as lines() describes it */
data class Line(
    val row: Int,
    val id: Long,
    val comment: Boolean,
    val layer: Int,
    val start: Int,
    val end: Int,
    val style: String,
    val actor: String,
    val effect: String,
    val marginL: Int,
    val marginR: Int,
    val marginV: Int,
    val text: String,
    /** Characters per second, or -1 when the grid shows none */
    val cps: Int,
    /** The grid's fold column: v, > or -, indented by depth */
    val fold: String,
    val foldOpener: Boolean,
    /** False when the line is in a closed fold */
    val visible: Boolean,
) {
    companion object {
        fun from(o: JSONObject) = Line(
            row = o.int("row"),
            id = o.optLong("id"),
            comment = o.bool("comment"),
            layer = o.int("layer"),
            start = o.int("start"),
            end = o.int("end"),
            style = o.str("style"),
            actor = o.str("actor"),
            effect = o.str("effect"),
            marginL = o.int("margin_l"),
            marginR = o.int("margin_r"),
            marginV = o.int("margin_v"),
            text = o.str("text"),
            cps = o.int("cps", -1),
            fold = o.str("fold"),
            foldOpener = o.bool("fold_opener"),
            visible = o.bool("visible", true),
        )
    }
}

/** The open file, as file_info() describes it */
data class FileInfo(
    val path: String = "",
    val modified: Boolean = false,
    val canUndo: Boolean = false,
    val canRedo: Boolean = false,
    val undo: String = "",
    val redo: String = "",
    val lines: Int = 0,
    val styles: Int = 0,
    val resX: Int = 0,
    val resY: Int = 0,
    val title: String = "",
    val activeRow: Int = -1,
) {
    /** The name the desktop program shows in its title bar */
    val displayName: String
        get() = if (path.isEmpty()) "Untitled" else path.substringAfterLast('/')

    companion object {
        fun from(o: JSONObject) = FileInfo(
            path = o.str("path"),
            modified = o.bool("modified"),
            canUndo = o.bool("can_undo"),
            canRedo = o.bool("can_redo"),
            undo = o.str("undo"),
            redo = o.str("redo"),
            lines = o.int("lines"),
            styles = o.int("styles"),
            resX = o.int("res_x"),
            resY = o.int("res_y"),
            title = o.str("title"),
            activeRow = o.int("active_row", -1),
        )
    }
}

/** What the edit box shows, from edit_box() and "edit_box" events */
data class EditBoxState(
    val active: Boolean = false,
    val row: Int = -1,
    val text: String = "",
    val start: Int = 0,
    val end: Int = 0,
    val duration: Int = 0,
    val layer: Int = 0,
    val comment: Boolean = false,
    val style: String = "",
    val actor: String = "",
    val effect: String = "",
    val marginL: Int = 0,
    val marginR: Int = 0,
    val marginV: Int = 0,
    val styles: List<String> = emptyList(),
    val actors: List<String> = emptyList(),
    val effects: List<String> = emptyList(),
    val styleExists: Boolean = true,
    val charCount: Int = 0,
    val charLimitExceeded: Boolean = false,
    val initialText: String = "",
    val showOriginal: Boolean = false,
    val selected: Int = 0,
) {
    companion object {
        fun from(o: JSONObject) = EditBoxState(
            active = o.bool("active"),
            row = o.int("row", -1),
            text = o.str("text"),
            start = o.int("start"),
            end = o.int("end"),
            duration = o.int("duration"),
            layer = o.int("layer"),
            comment = o.bool("comment"),
            style = o.str("style"),
            actor = o.str("actor"),
            effect = o.str("effect"),
            marginL = o.int("margin_l"),
            marginR = o.int("margin_r"),
            marginV = o.int("margin_v"),
            styles = o.strings("styles"),
            actors = o.strings("actors"),
            effects = o.strings("effects"),
            styleExists = o.bool("style_exists", true),
            charCount = o.int("char_count"),
            charLimitExceeded = o.bool("char_limit_exceeded"),
            initialText = o.str("initial_text"),
            showOriginal = o.bool("show_original"),
            selected = o.int("selected"),
        )
    }
}

/** A style, as the style methods describe it */
data class AssStyle(
    val name: String = "Default",
    val font: String = "Arial",
    val size: Double = 48.0,
    val primary: String = "&H00FFFFFF",
    val secondary: String = "&H000000FF",
    val outline: String = "&H00000000",
    val shadow: String = "&H00000000",
    val bold: Boolean = false,
    val italic: Boolean = false,
    val underline: Boolean = false,
    val strikeout: Boolean = false,
    val scaleX: Double = 100.0,
    val scaleY: Double = 100.0,
    val spacing: Double = 0.0,
    val angle: Double = 0.0,
    val borderStyle: Int = 1,
    val outlineWidth: Double = 2.0,
    val shadowWidth: Double = 2.0,
    val alignment: Int = 2,
    val marginL: Int = 10,
    val marginR: Int = 10,
    val marginV: Int = 10,
    val encoding: Int = 1,
) {
    fun toJson(): JSONObject = JSONObject()
        .put("name", name)
        .put("font", font)
        .put("size", size)
        .put("primary", primary)
        .put("secondary", secondary)
        .put("outline", outline)
        .put("shadow", shadow)
        .put("bold", bold)
        .put("italic", italic)
        .put("underline", underline)
        .put("strikeout", strikeout)
        .put("scale_x", scaleX)
        .put("scale_y", scaleY)
        .put("spacing", spacing)
        .put("angle", angle)
        .put("border_style", borderStyle)
        .put("outline_width", outlineWidth)
        .put("shadow_width", shadowWidth)
        .put("alignment", alignment)
        .put("margin_l", marginL)
        .put("margin_r", marginR)
        .put("margin_v", marginV)
        .put("encoding", encoding)

    companion object {
        fun from(o: JSONObject) = AssStyle(
            name = o.str("name"),
            font = o.str("font"),
            size = o.double("size"),
            primary = o.str("primary"),
            secondary = o.str("secondary"),
            outline = o.str("outline"),
            shadow = o.str("shadow"),
            bold = o.bool("bold"),
            italic = o.bool("italic"),
            underline = o.bool("underline"),
            strikeout = o.bool("strikeout"),
            scaleX = o.double("scale_x", 100.0),
            scaleY = o.double("scale_y", 100.0),
            spacing = o.double("spacing"),
            angle = o.double("angle"),
            borderStyle = o.int("border_style", 1),
            outlineWidth = o.double("outline_width"),
            shadowWidth = o.double("shadow_width"),
            alignment = o.int("alignment", 2),
            marginL = o.int("margin_l"),
            marginR = o.int("margin_r"),
            marginV = o.int("margin_v"),
            encoding = o.int("encoding", 1),
        )

        fun list(o: JSONObject) = o.objects("styles").map(::from)
    }
}

/** A macro of an automation script */
data class Macro(val command: String, val name: String, val help: String)

/** An automation script, as macros() describes it */
data class AutomationScript(
    val path: String,
    val file: String,
    val name: String,
    val description: String,
    val author: String,
    val version: String,
    val loaded: Boolean,
    val warnings: List<String>,
    /** One of the user's own scripts, rather than one which comes with the app */
    val custom: Boolean,
    /** Loaded with the open subtitles rather than for every file */
    val local: Boolean,
    val macros: List<Macro>,
) {
    companion object {
        fun from(o: JSONObject, local: Boolean) = AutomationScript(
            path = o.str("path"),
            file = o.str("file"),
            name = o.str("name"),
            description = o.str("description"),
            author = o.str("author"),
            version = o.str("version"),
            loaded = o.bool("loaded"),
            warnings = o.strings("warnings"),
            custom = o.bool("custom"),
            local = local,
            macros = o.objects("macros").map { Macro(it.str("command"), it.str("name"), it.str("help")) },
        )

        fun list(o: JSONObject) =
            o.objects("global").map { from(it, false) } + o.objects("local").map { from(it, true) }
    }
}

/** One of the program's commands, from commands() */
data class CommandInfo(
    val name: String,
    val menu: String,
    val display: String,
    val help: String,
    val validate: Boolean,
    val toggle: Boolean,
    val radio: Boolean,
    val dynamicName: Boolean,
) {
    companion object {
        fun from(o: JSONObject) = CommandInfo(
            name = o.str("name"),
            menu = o.str("menu"),
            display = o.str("display"),
            help = o.str("help"),
            validate = o.bool("validate"),
            toggle = o.bool("toggle"),
            radio = o.bool("radio"),
            dynamicName = o.bool("dynamic_name"),
        )
    }
}
