param(
    [ValidateSet('Debug', 'Release', 'Sanitizer', 'All')]
    [string]$Config = 'All'
)

. "$PSScriptRoot\build_common.ps1"

Build-GLFW -Config $Config
Build-Slang -Config $Config

Log 'Finished building lib dependencies.'
