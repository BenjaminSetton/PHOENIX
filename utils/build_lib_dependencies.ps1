param(
    [ValidateSet('Debug', 'Release', 'Sanitizer', 'All')]
    [string]$Config = 'All',
    [string]$Architecture = 'x64'
)

. "$PSScriptRoot\build_common.ps1"

Build-GLFW -Config $Config -Architecture $Architecture
Build-Slang -Config $Config -Architecture $Architecture

Log 'Finished building lib dependencies.'
