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

  set(_patch_check_args --check --recount --ignore-space-change)
  set(_patch_apply_args --recount --ignore-space-change)

  execute_process(
    COMMAND ${GIT_EXECUTABLE} -C ${repo_dir} apply ${_patch_check_args} ${patch_file}
    RESULT_VARIABLE _patch_check_result
    OUTPUT_QUIET
    ERROR_QUIET)
  if(_patch_check_result EQUAL 0)
    message(STATUS "Applying ${patch_name}")
    execute_process(
      COMMAND ${GIT_EXECUTABLE} -C ${repo_dir} apply ${_patch_apply_args} ${patch_file}
      RESULT_VARIABLE _patch_apply_result)
    if(NOT _patch_apply_result EQUAL 0)
      message(FATAL_ERROR "Failed to apply ${patch_name}")
    endif()
    return()
  endif()

  execute_process(
    COMMAND ${GIT_EXECUTABLE} -C ${repo_dir} apply --reverse ${_patch_check_args} ${patch_file}
    RESULT_VARIABLE _patch_reverse_result
    OUTPUT_QUIET
    ERROR_QUIET)
  if(_patch_reverse_result EQUAL 0)
    message(STATUS "${patch_name} already applied")
    return()
  endif()

  execute_process(
    COMMAND ${GIT_EXECUTABLE} -C ${repo_dir} apply --3way ${_patch_apply_args} ${patch_file}
    RESULT_VARIABLE _patch_three_way_result
    OUTPUT_QUIET
    ERROR_QUIET)
  if(_patch_three_way_result EQUAL 0)
    message(STATUS "Applied ${patch_name} with 3-way merge")
    return()
  endif()

  message(FATAL_ERROR "${patch_name} could not be applied cleanly to ${repo_dir}; patch is neither directly applicable nor already present")
endfunction()
