// Forward declaration of the script-adding function.
extern "C" void AddSC_mod_chat_filter();

// This function is called by the AzerothCore build system to initialize the module's scripts.
void Addmod_chat_filterScripts()
{
    AddSC_mod_chat_filter();
}
