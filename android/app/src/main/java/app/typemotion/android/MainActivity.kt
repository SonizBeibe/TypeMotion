package app.typemotion.android

import android.os.Bundle
import androidx.activity.ComponentActivity
import androidx.activity.compose.setContent
import androidx.activity.enableEdgeToEdge
import app.typemotion.ui.TypeMotionApp

class MainActivity : ComponentActivity() {
    private val typeMotion get() = application as TypeMotionApplication

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        enableEdgeToEdge()
        typeMotion.platform.activity = this
        val app = typeMotion.app
        app.start()
        setContent { TypeMotionApp(app) }
    }

    override fun onResume() {
        super.onResume()
        typeMotion.platform.activity = this
        typeMotion.app.onResume()
    }

    override fun onStop() {
        super.onStop()
        // Android can stop the process from here on without telling the app
        typeMotion.app.flush()
    }

    override fun onDestroy() {
        if (typeMotion.platform.activity === this) typeMotion.platform.activity = null
        super.onDestroy()
    }

    @Deprecated("Deprecated in Java")
    override fun onRequestPermissionsResult(requestCode: Int, permissions: Array<String>, grantResults: IntArray) {
        super.onRequestPermissionsResult(requestCode, permissions, grantResults)
        typeMotion.app.onResume()
    }
}
