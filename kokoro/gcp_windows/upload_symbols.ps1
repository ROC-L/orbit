function install_oauth2l {
  if (-Not (Test-Path 'oauth2l.tgz' -PathType Leaf)) {
    Invoke-WebRequest -Uri https://storage.googleapis.com/oauth2l/latest/windows_amd64.tgz -OutFile oauth2l.tgz
  }

  $oauth2l_path = Join-Path -Path $pwd -ChildPath "oauth2l"
  if (-Not (Test-Path ($oauth2l_path + '\oauth2l.exe') -PathType Leaf)) {
    New-Item -Path . -Name "oauth2l" -ItemType "directory" -errorAction SilentlyContinue
    tar -zxvf oauth2l.tgz -C $oauth2l_path --strip-components 1
	Rename-Item -Path ($oauth2l_path + '\oauth2l') -NewName ($oauth2l_path + '\oauth2l.exe')
  }
}

function get_authorization_header {
  $authorization = oauth2l\oauth2l.exe header cloud-platform userinfo.email
  $authorization = $authorization -Replace ': ', ' = ' | ConvertFrom-StringData
  return $authorization
}

function get_crash_symbol_collector_key_name($authorization) {
  $keys = Invoke-RestMethod -Header $authorization  -ContentType "application/json" -Uri "https://apikeys.googleapis.com/v2alpha1/projects/60941241589/keys"
  if (-Not $keys.keys) {
    Write-Host $keys
    Throw "Can't get list of API keys"
  }
  
  $key = $keys.keys | where {$_.displayName -eq "Crash Symbol Collector API key"}
  if (-Not $key) { Throw "'Crash Symbol Collector API key' not found" }
  
  return $key.name
}

function get_api_key_by_key_name($authorization, $key_full_name) {
  $key = Invoke-RestMethod -Header $authorization  -ContentType "application/json" -uri "https://apikeys.googleapis.com/v2alpha1/$key_full_name/keyString"
  return $key.keyString
}

function get_crash_symbol_collector_api_key {
  $authorization = get_authorization_header
  $crash_symbol_key_full_name = get_crash_symbol_collector_key_name $authorization
  $crash_symbol_collector_api_key = get_api_key_by_key_name $authorization $crash_symbol_key_full_name
  
  return $crash_symbol_collector_api_key
}

function install_breakpad_tools {
  if (-Not (Test-Path -Path 'breakpad' -PathType Container)) {
    conan install -g deploy breakpad/2ffe116@orbitdeps/stable
  }
}

function upload_symbol_file($api_key, $symbol_file_path) {
  breakpad\bin\symupload -p $symbol_file_path https://prod-crashsymbolcollector-pa.googleapis.com $api_key
}

function upload_debug_symbols($bin_folder) {
  install_oauth2l

  $api_key = get_crash_symbol_collector_api_key
  if (-Not $api_key) { Throw "Error on getting API key" }
  
  install_breakpad_tools
  
  upload_symbol_file $api_key $bin_folder\Orbit.exe
  upload_symbol_file $api_key $bin_folder\Qt5Core.dll
  upload_symbol_file $api_key $bin_folder\Qt5Gui.dll
  upload_symbol_file $api_key $bin_folder\Qt5Network.dll
  upload_symbol_file $api_key $bin_folder\Qt5Widgets.dll
  upload_symbol_file $api_key $bin_folder\bearer\qgenericbearer.dll
  upload_symbol_file $api_key $bin_folder\imageformats\qgif.dll
  upload_symbol_file $api_key $bin_folder\imageformats\qico.dll
  upload_symbol_file $api_key $bin_folder\imageformats\qjpeg.dll
  upload_symbol_file $api_key $bin_folder\platforms\qwindows.dll
  upload_symbol_file $api_key $bin_folder\styles\qwindowsvistastyle.dll
}

upload_debug_symbols $args[0]
