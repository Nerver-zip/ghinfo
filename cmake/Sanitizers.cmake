function(ghinfo_enable_sanitizers target)
    if(NOT GHINFO_ENABLE_SANITIZERS)
        return()
    endif()

    if(MSVC)
        message(WARNING "GHINFO_ENABLE_SANITIZERS is not configured for MSVC")
        return()
    endif()

    target_compile_options(${target} PRIVATE -fsanitize=address,undefined -fno-omit-frame-pointer)
    target_link_options(${target} PRIVATE -fsanitize=address,undefined -fno-omit-frame-pointer)
endfunction()
