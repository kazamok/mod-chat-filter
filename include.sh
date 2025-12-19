# Add your custom script to the cmake build system
# ------------------------------------------------
# add_custom_script(SCRIPNAME SCRIPT_FILES)
#
# SCRIPNAME - is the name of your script so that it can be identified by the build system.
# SCRIPT_FILES - are the files that should be compiled, this can be a list of files.
#
# EXAMPLE: add_custom_script(my_cool_script "my_cool_script.cpp")
#
# You can add more than one file by creating a list of files.
# EXAMPLE: add_custom_script(my_cool_script "my_cool_script.cpp;my_other_script.cpp")

add_custom_script(mod_chat_filter "mod_chat_filter_loader.cpp;ChatFilter.cpp")
