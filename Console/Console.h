/// @brief set up the listening thread
/// @return true if listener thread was created
bool SetupListener(std::string cmdService = "2000");

/// @brief terminate the listening thread
void TerminateListener();

/// @brief Main Loop continually called by ImGui to display the selected windows of the console
void MainLoop();