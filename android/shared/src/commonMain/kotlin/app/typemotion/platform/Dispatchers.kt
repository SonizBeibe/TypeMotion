package app.typemotion.platform

import kotlinx.coroutines.CoroutineDispatcher

/** For reading and writing files: Dispatchers.IO where there is one */
expect val IoDispatcher: CoroutineDispatcher
