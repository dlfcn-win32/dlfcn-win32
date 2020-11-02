#
# provide option with three states AUTO, ON, OFF
#
macro(add_auto_option _name _text _default)
    if(NOT DEFINED ${_name})
        set(${_name} ${_default} CACHE STRING "${_text}" FORCE)
    else()
        set(${_name} ${_default} CACHE STRING "${_text}")
    endif()
    set_property(CACHE ${_name} PROPERTY STRINGS AUTO ON OFF)
endmacro()

#
# Ensure that if a tristate ON/OFF/AUTO feature is set to ON,
# its requirements have been met. Fail with a fatal error if not.
#
# _name: name of a variable ENABLE_FOO representing a tristate ON/OFF/AUTO feature
# _text: human-readable description of the feature enabled by _name, for the
#        error message
# _var: name of a variable representing a system property we checked for,
#       such as an executable that must exist for the feature enabled by _name to work
# _vartext: what we checked for, for the error message
#
macro(check_auto_option _name _text _var _vartext)
    set(_nameval ${${_name}})
    set(_varval ${${_var}})
    #message("debug: _name ${_name} ${_nameval}  _var ${_var} ${_varval}")
    if(_nameval AND NOT _nameval STREQUAL "AUTO" AND NOT _varval)
        message(FATAL_ERROR "${_text} requested but ${_vartext} not found")
    endif()
endmacro()

