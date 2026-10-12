# T1.4: uc2_core is the Kconfig home for all UC2_* feature symbols (Kconfig
# next to this file; sourced into the build via uc2_app/Kconfig).
#
# Sources (state, config, qid, message, buzzer) move in from main/src in T1.6
# and gate per refs/component-layout.md §3, e.g.:
#   if(CONFIG_UC2_MESSAGE_CONTROLLER)
#       list(APPEND srcs message/MessageController.cpp)
#   endif()
# Until then this placeholder TU keeps the component well-formed.
idf_component_register(SRCS "uc2_core.c" INCLUDE_DIRS ".")
