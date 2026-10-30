package app.typemotion.state

import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.setValue
import androidx.compose.ui.text.TextRange
import androidx.compose.ui.text.input.TextFieldValue
import app.typemotion.engine.EngineError
import app.typemotion.engine.int
import app.typemotion.model.EditBoxState
import app.typemotion.model.Utf8
import kotlinx.coroutines.CoroutineStart
import kotlinx.coroutines.launch
import org.json.JSONObject

/**
 * The edit box: the active line's fields and the text field. Edits go to
 * the engine as they are made, which changes all the selected lines like the
 * desktop edit box; the engine's own changes to the line come back as
 * "edit_box" events.
 */
class EditBoxController(private val app: AppState) {
    var state by mutableStateOf(EditBoxState())
        private set

    /** The text field's text, cursor and selection */
    var value by mutableStateOf(TextFieldValue(""))
        private set

    /** Edits sent and not yet done, during which the engine's text is older than ours */
    private var inFlight = 0

    /** Where the cursor was after the last typed edit, to tell typing from a new edit */
    private var lastEditCaret = -1

    /** The engine's cursor, in bytes: start, end, insertion */
    private var engineSelection = Triple(0, 0, 0)

    suspend fun load() {
        val result = app.engine.call("edit_box")
        state = EditBoxState.from(result)
        value = TextFieldValue(state.text, TextRange(state.text.length))
        lastEditCaret = -1
    }

    fun onState(data: JSONObject) {
        state = EditBoxState.from(data)
        if (data.optBoolean("text_changed") && inFlight == 0) {
            value = TextFieldValue(state.text, engineRange(state.text))
            lastEditCaret = -1
        }
    }

    fun onTextSelection(data: JSONObject) {
        engineSelection = Triple(data.int("start"), data.int("end"), data.int("insertion"))
        if (inFlight == 0)
            value = value.copy(selection = engineRange(value.text))
    }

    private fun engineRange(text: String): TextRange {
        val (start, end, insertion) = engineSelection
        val s = Utf8.charIndex(text, start)
        val e = Utf8.charIndex(text, end)
        // The cursor is at the insertion point, which the range ends at
        return if (insertion == start && start != end) TextRange(e, s) else TextRange(s, e)
    }

    /** The text field changed: typing, or moving the cursor */
    fun onValueChange(new: TextFieldValue) {
        val old = value
        value = new
        if (!state.active) return

        if (new.text != old.text) {
            // Typing at the cursor continues the undo step, like the desktop
            // edit control's typing does
            val continues = old.selection.collapsed && old.selection.start == lastEditCaret
            lastEditCaret = new.selection.end
            send("edit_box_set", JSONObject()
                .put("field", "text")
                .put("value", new.text)
                .put("new_action", !continues)) { state = EditBoxState.from(it) }
            sendSelection(new)
        }
        else if (new.selection != old.selection) {
            lastEditCaret = -1
            sendSelection(new)
        }
    }

    private fun sendSelection(v: TextFieldValue) {
        val text = v.text
        send("set_text_selection", JSONObject()
            .put("start", Utf8.byteOffset(text, v.selection.min))
            .put("end", Utf8.byteOffset(text, v.selection.max))
            .put("insertion", Utf8.byteOffset(text, v.selection.end)))
    }

    /** Change one of the other fields: start, end, duration, layer, comment, style, actor, effect, margin_l/r/v */
    fun set(field: String, fieldValue: Any, typed: Boolean = false) {
        send("edit_box_set", JSONObject()
            .put("field", field)
            .put("value", fieldValue)
            .put("typed", typed)) { state = EditBoxState.from(it) }
    }

    /**
     * Send in order: the calls run one after another, in the order they
     * were started
     */
    private fun send(method: String, args: JSONObject, onResult: ((JSONObject) -> Unit)? = null) {
        inFlight++
        app.scope.launch(start = CoroutineStart.UNDISPATCHED) {
            try {
                val result = app.engine.call(method, args)
                onResult?.invoke(result)
            } catch (e: EngineError) {
                app.errorMessage = e.message
            } finally {
                inFlight--
            }
        }
    }
}
