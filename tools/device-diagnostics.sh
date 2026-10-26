# Source from installers. Raw discovery is captured locally; all other ADB
# output is redacted before it reaches the terminal, tee, or a failure message.
PRIVATE_DEVICE_IDS=()
protect_device_diagnostic() {
  local text="$1" id i j swap
  local ids=("${PRIVATE_DEVICE_IDS[@]}" "${SERIAL:-}")
  # Replace longer IDs first so an overlapping shorter ID cannot leave a suffix.
  for ((i=0; i<${#ids[@]}; i++)); do
    for ((j=i+1; j<${#ids[@]}; j++)); do
      if [ "${#ids[j]}" -gt "${#ids[i]}" ]; then
        swap="${ids[i]}"; ids[i]="${ids[j]}"; ids[j]="$swap"
      fi
    done
  done
  for id in "${ids[@]}"; do
    [ -n "$id" ] || continue
    # Quoted pattern expansion treats punctuation in a supplied ID literally.
    text="${text//"$id"/personal Quest 3 device}"
  done
  printf '%s\n' "$text"
}

private_device_label() {
  local line model='Android device'
  while IFS= read -r line; do
    if [[ "$line" == "$1 "* || "$line" == "$1"$'\t'* ]]; then
      if [[ "$line" =~ model:([^[:space:]]+) ]]; then model="${BASH_REMATCH[1]//_/ }"; fi
    fi
  done <<< "${CAPTURED_OUTPUT:-}"
  protect_device_diagnostic "$model"
}

private_adb() {
  local output rc
  output="$("$ADB" "$@" 2>&1)"
  rc=$?
  protect_device_diagnostic "$output"
  return "$rc"
}

private_adb_discover() {
  # Must be called in the parent shell so the redaction list survives.
  CAPTURED_OUTPUT="$("$ADB" devices -l 2>&1)"
  CAPTURED_EXIT=$?
  local line
  while IFS= read -r line; do
    if [[ "$line" =~ ^([^[:space:]]+)[[:space:]]+(device|offline|unauthorized|recovery|sideload)([[:space:]]|$) ]]; then
      PRIVATE_DEVICE_IDS+=("${BASH_REMATCH[1]}")
    fi
  done <<< "$CAPTURED_OUTPUT"
}
