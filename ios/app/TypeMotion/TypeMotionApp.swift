import SwiftUI
import TypeMotionShared

/// The iPhone app: the same screens as the Android app, from the Kotlin
/// module android/shared, over the same C++ engine
@main
struct TypeMotionApp: App {
    var body: some Scene {
        WindowGroup {
            ComposeView()
                .ignoresSafeArea(.all)
        }
    }
}

struct ComposeView: UIViewControllerRepresentable {
    func makeUIViewController(context: Context) -> UIViewController {
        MainViewControllerKt.MainViewController()
    }

    func updateUIViewController(_ uiViewController: UIViewController, context: Context) {
    }
}
