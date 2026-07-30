param(
    [ValidateSet('Debug', 'Release', 'Sanitizer', 'All')]
    [string]$Config = 'All'
)

. "$PSScriptRoot\build_common.ps1"

Build-Assimp -Config $Config
Build-GLM -Config $Config

Log 'Finished building sample dependencies.'
