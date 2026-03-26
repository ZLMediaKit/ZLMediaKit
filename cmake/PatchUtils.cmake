function(zlm_apply_git_patch repo_dir patch_file patch_name)
  if(NOT GIT_FOUND)
    find_package(Git QUIET)
  endif()
  if(NOT GIT_FOUND)
    message(FATAL_ERROR "Applying ${patch_name} requires Git")
  endif()

  if(NOT EXISTS "${patch_file}")
    message(FATAL_ERROR "Patch file not found: ${patch_file}")
  endif()

  execute_process(
    COMMAND ${GIT_EXECUTABLE} -C ${repo_dir} apply --check ${patch_file}
    RESULT_VARIABLE _patch_check_result
    OUTPUT_QUIET
    ERROR_QUIET)
  if(_patch_check_result EQUAL 0)
    message(STATUS "Applying ${patch_name}")
    execute_process(
      COMMAND ${GIT_EXECUTABLE} -C ${repo_dir} apply ${patch_file}
      RESULT_VARIABLE _patch_apply_result)
    if(NOT _patch_apply_result EQUAL 0)
      message(FATAL_ERROR "Failed to apply ${patch_name}")
    endif()
    return()
  endif()

  execute_process(
    COMMAND ${GIT_EXECUTABLE} -C ${repo_dir} apply --reverse --check ${patch_file}
    RESULT_VARIABLE _patch_reverse_result
    OUTPUT_QUIET
    ERROR_QUIET)
  if(_patch_reverse_result EQUAL 0)
    message(STATUS "${patch_name} already applied")
    return()
  endif()

  message(FATAL_ERROR "${patch_name} could not be applied cleanly to ${repo_dir}")
endfunction()
