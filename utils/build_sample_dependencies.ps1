param(
    [ValidateSet('Debug', 'Release', 'Sanitizer', 'All')]
    [string]$Config = 'All',
    [string]$Architecture = 'x64'
)

. "$PSScriptRoot\build_common.ps1"

Build-Assimp -Config $Config -Architecture $Architecture
Build-GLM -Config $Config -Architecture $Architecture

Log 'Finished building sample dependencies.'
